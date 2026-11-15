"""Exercise the real playback functions without loading credentials or using Spotify."""
import ast
import json
import tempfile
import time
import unittest
from pathlib import Path
from flask import Flask, jsonify, request
from spotipy.oauth2 import SpotifyOAuth


class FakeSpotify:
    def __init__(self, devices):
        self.available = devices
        self.started = []
        self.fail = False

    def devices(self):
        return {"devices": self.available}

    def start_playback(self, **kwargs):
        if self.fail:
            raise RuntimeError("test playback rejection")
        self.started.append(kwargs)

    def volume(self, value, **kwargs):
        pass


class PlaybackTest(unittest.TestCase):
    def setUp(self):
        source = Path(__file__).resolve().parents[2] / "services/spotify_scraper_service.py"
        names = {"_connect_device_id", "_ensure_full_volume", "PlaybackError", "_start_local_playback",
                 "_playback_error_response", "_play_at", "play_track", "play_context", "pause_player"}
        nodes = [n for n in ast.parse(source.read_text()).body
                 if isinstance(n, (ast.FunctionDef, ast.ClassDef)) and n.name in names]
        self.api = FakeSpotify([])
        self.app = Flask(__name__)
        self.state = {"is_playing": False, "queue": [], "queue_pos": -1}
        self.scope = {"app": self.app, "jsonify": jsonify, "request": request, "time": time,
                      "is_logged_in": lambda: True, "sp_official": self.api, "SPOTIFYD_NAME": "",
                      "player_state": self.state, "_status_cache": {}, "_vol_done": set(),
                      "_spotify_call": lambda fn, *a, **kw: fn(*a, **kw),
                      "_uri": lambda tid: "spotify:track:" + tid,
                      "_push_recent": lambda t: None, "_radio_enqueue_bg": lambda *a: None}
        exec(compile(ast.Module(body=nodes, type_ignores=[]), str(source), "exec"), self.scope)
        self.client = self.app.test_client()

    def test_no_receiver_does_not_fake_playback(self):
        response = self.client.get("/play/test")
        self.assertEqual(response.status_code, 409)
        self.assertEqual(response.json["code"], "no_local_device")
        self.assertFalse(self.state["is_playing"])
        self.assertFalse(self.api.started)

    def test_active_phone_is_not_silently_selected(self):
        self.api.available = [{"id": "phone", "name": "Phone", "is_active": True}]
        self.assertEqual(self.client.get("/play/test").status_code, 409)
        self.assertFalse(self.api.started)

    def test_local_receiver_is_selected(self):
        self.api.available = [{"id": "phone", "name": "Phone", "is_active": True},
                              {"id": "local", "name": "Volta OS (spotifyd)"}]
        self.assertEqual(self.client.get("/play/test").status_code, 200)
        self.assertEqual(self.api.started[0]["device_id"], "local")
        self.assertTrue(self.state["is_playing"])

    def test_official_receiver_is_selected_explicitly(self):
        self.scope["SPOTIFYD_NAME"] = "Volta OS (soloist)"
        self.api.available = [{"id": "old", "name": "Volta OS (spotifyd)"},
                              {"id": "official", "name": "Volta OS (soloist)"}]
        self.assertEqual(self.client.get("/play/test").status_code, 200)
        self.assertEqual(self.api.started[0]["device_id"], "official")

    def test_api_rejection_is_reported(self):
        self.api.available = [{"id": "local", "name": "spotifyd"}]
        self.api.fail = True
        response = self.client.get("/play/test")
        self.assertEqual(response.status_code, 502)
        self.assertEqual(response.json["status"], "error")
        self.assertFalse(self.state["is_playing"])

    def test_playlist_failure_preserves_queue(self):
        response = self.client.post("/play_context", json={"tracks": [{"id": "test"}], "index": 0})
        self.assertEqual(response.status_code, 409)
        self.assertEqual(self.state["queue"], [])

    def test_resume_requires_local_receiver(self):
        self.assertEqual(self.client.get("/player/pause?state=0").status_code, 409)
        self.assertFalse(self.state["is_playing"])

    def test_login_is_required(self):
        self.scope["is_logged_in"] = lambda: False
        self.assertEqual(self.client.get("/play/test").status_code, 401)

    def test_idle_receiver_retries_with_album_context(self):
        class RestrictionError(Exception):
            http_status = 403
            msg = "Player command failed: Restriction violated"
        self.api.available = [{"id": "local", "name": "spotifyd"}]
        def start(**kwargs):
            if "uris" in kwargs:
                raise RestrictionError()
            self.api.started.append(kwargs)
        self.api.start_playback = start
        self.api.track = lambda uri: {"album": {"uri": "spotify:album:test-album"}}
        response = self.client.get("/play/test?start_ms=5000")
        self.assertEqual(response.status_code, 200)
        self.assertEqual(self.api.started, [{"device_id": "local", "context_uri": "spotify:album:test-album",
                                            "offset": {"uri": "spotify:track:test"}, "position_ms": 5000}])

    def test_other_403_is_not_bypassed(self):
        class ForbiddenError(Exception):
            http_status = 403
            msg = "Premium required"
        self.api.available = [{"id": "local", "name": "spotifyd"}]
        def start(**kwargs):
            raise ForbiddenError()
        self.api.start_playback = start
        response = self.client.get("/play/test")
        self.assertEqual(response.status_code, 502)
        self.assertFalse(self.state["is_playing"])


class AuthTest(unittest.TestCase):
    def test_missing_scope_metadata_is_not_reported_as_logged_in(self):
        source = Path(__file__).resolve().parents[2] / "services/spotify_scraper_service.py"
        nodes = [n for n in ast.parse(source.read_text()).body
                 if isinstance(n, ast.FunctionDef) and n.name == "is_logged_in"]
        with tempfile.TemporaryDirectory() as directory:
            token = Path(directory) / "token.json"
            scope = {"sp_oauth": object(), "TOKEN_PATH": str(token), "SCOPE": "test-read test-play"}
            exec(compile(ast.Module(body=nodes, type_ignores=[]), str(source), "exec"), scope)
            token.write_text(json.dumps({"refresh_token": "dummy", "scope": None}))
            self.assertFalse(scope["is_logged_in"]())
            token.write_text(json.dumps({"refresh_token": "dummy", "scope": "test-read test-play"}))
            self.assertTrue(scope["is_logged_in"]())

    def test_background_auth_never_prompts_on_stdin(self):
        source = Path(__file__).resolve().parents[2] / "services/spotify_scraper_service.py"
        nodes = [n for n in ast.parse(source.read_text()).body
                 if isinstance(n, ast.ClassDef) and n.name == "NonInteractiveSpotifyOAuth"]
        scope = {"SpotifyOAuth": SpotifyOAuth}
        exec(compile(ast.Module(body=nodes, type_ignores=[]), str(source), "exec"), scope)
        auth_type = scope["NonInteractiveSpotifyOAuth"]
        auth = object.__new__(auth_type)
        with self.assertRaisesRegex(RuntimeError, "login_required"):
            auth.get_auth_response()


if __name__ == "__main__":
    unittest.main()
