#!/usr/bin/env python3
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class SceneEditorMeshPickScrollContractTest(unittest.TestCase):
    def test_hover_and_click_share_geometry_query_without_origin_fallback(self):
        for name in ("scene_editor_chrome_actions.c", "scene_editor_digest_overlay.c"):
            source = (ROOT / "src/editor" / name).read_text()
            self.assertIn("SceneEditorViewportPickObject", source)
            self.assertNotIn("SceneEditorDigestOverlayPickObjectIndex", source)
        chrome = (ROOT / "src/editor/scene_editor_chrome_actions.c").read_text()
        self.assertNotIn("pick = *env->digest_hover_object_index", chrome)

    def test_object_list_uses_shared_scroll_contract_and_clipped_hit_rows(self):
        object_list = (ROOT / "src/editor/scene_editor_object_list.c").read_text()
        self.assertIn("kit_ui_eval_scroll", object_list)
        # The outliner uses ordinary bounded scrolling, not top-anchor padding
        # that lets the last row scroll into otherwise empty trailing space.
        self.assertIn("SDL_RenderSetClipRect", object_list)
        self.assertIn("ObjectEditorRegisterObjectListRow", object_list)

    def test_object_input_routes_list_wheel_before_other_panel_scrolling(self):
        source = (ROOT / "src/editor/object_editor_input.c").read_text()
        list_wheel = source.find("SceneEditorObjectListHandleWheel")
        asset_scroll = source.find("ObjectEditorPanels_AssetMaxScroll", list_wheel)
        self.assertGreaterEqual(list_wheel, 0)
        self.assertGreater(asset_scroll, list_wheel)


if __name__ == "__main__":
    unittest.main()
