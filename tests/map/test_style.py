import copy
import json
import sys
import unittest
from pathlib import Path
from unittest.mock import patch
from urllib.parse import urlparse, parse_qs

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "services"))
import map_proxy


class StyleTests(unittest.TestCase):
    def test_public_styles_work_with_account_key(self):
        style = {"version": 8, "sources": {"roads": {"type": "vector", "url": "mapbox://roads"}},
                 "layers": [{"id": "background", "type": "background"}]}
        with patch.dict("os.environ", {}, clear=True), patch.object(map_proxy, "MAPTILER_KEY", "test-key"):
            for theme, expected in [("light", "streets-v2"), ("dark", "dataviz-dark")]:
                with self.subTest(theme=theme), patch.object(map_proxy, "_fetch_cached", return_value=(json.dumps(copy.deepcopy(style)).encode(), 200)) as fetch:
                    response = map_proxy.app.test_client().get("/style.json?s=" + theme)
                    self.assertEqual(response.status_code, 200)
                    url = urlparse(fetch.call_args.args[2])
                    self.assertEqual(url.path, "/maps/" + expected + "/style.json")
                    self.assertEqual(parse_qs(url.query)["key"], ["test-key"])
                    self.assertEqual(response.json["sources"]["roads"]["url"], "http://127.0.0.1:9098/tilejson/roads")
                    self.assertTrue(response.json["layers"])

    def test_upstream_error_is_actionable_without_leaking_credentials(self):
        with patch.object(map_proxy, "_fetch_cached", return_value=(None, 403)):
            response = map_proxy.app.test_client().get("/style.json?s=light")
        self.assertEqual(response.status_code, 502)
        self.assertEqual(response.json, {"error": "map style unreachable", "upstream_status": 403})

    def test_bad_upstream_json_is_a_gateway_error(self):
        for body in [b"not json", b"[]", b'{"error":"denied"}']:
            with self.subTest(body=body), patch.object(map_proxy, "_fetch_cached", return_value=(body, 200)):
                response = map_proxy.app.test_client().get("/style.json")
                self.assertEqual(response.status_code, 502)


if __name__ == "__main__":
    unittest.main()
