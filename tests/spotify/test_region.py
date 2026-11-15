"""Country selection uses mocked geocoding and searches, without account access."""
import ast
import time
import unittest
from pathlib import Path
from types import SimpleNamespace
from flask import Flask, jsonify, request

ROOT = Path(__file__).resolve().parents[2]

def load_function(path, name, scope):
    node = next(n for n in ast.parse(path.read_text()).body
                if isinstance(n, ast.FunctionDef) and n.name == name)
    exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), 'exec'), scope)

class RegionTest(unittest.TestCase):
    def test_home_searches_follow_country_and_validate_input(self):
        app = Flask(__name__)
        calls = []
        scope = dict(app=app, jsonify=jsonify, request=request, _recents=[],
                     is_logged_in=lambda: False, sp_official=None,
                     sp_scraper=SimpleNamespace(search=lambda q: calls.append(q) or SimpleNamespace()),
                     _random=SimpleNamespace(choice=lambda choices: choices[0]), get_img=lambda x: '')
        load_function(ROOT/'services/spotify_scraper_service.py', 'home', scope)
        for code, expected in [('DE','Germany'), ('IT','Italy'), ('AT','Austria'), ('!!!','Germany')]:
            calls.clear()
            app.test_client().get('/ui/home?country='+code)
            self.assertIn('Top 50 '+expected, calls)
            self.assertIn('New releases '+expected, calls)
            self.assertIn('Playlist Pop '+expected, calls)

    def test_country_is_extracted_and_cached_with_address(self):
        app = Flask(__name__)
        payload = {'features': [{'text':'Teststraße', 'context': [
            {'id':'country.1','text':'Germany','country_code':'de'}]}]}
        response = SimpleNamespace(raise_for_status=lambda: None, json=lambda: payload)
        calls = []
        scope = dict(app=app, jsonify=jsonify, request=request, time=time,
                     _geocode_cache={}, _GEOCODE_CACHE_TTL=600, MAPTILER_KEY='test',
                     _http=SimpleNamespace(get=lambda *a, **kw: calls.append(a) or response))
        load_function(ROOT/'services/map_proxy.py', 'reverse_geocode', scope)
        client = app.test_client()
        for _ in range(2):
            data = client.get('/reverse-geocode?lat=52.52&lon=13.405').json
            self.assertEqual(data['country_code'], 'DE')
            self.assertEqual(data['country_name'], 'Germany')
        self.assertEqual(len(calls), 1)

if __name__ == '__main__': unittest.main()
