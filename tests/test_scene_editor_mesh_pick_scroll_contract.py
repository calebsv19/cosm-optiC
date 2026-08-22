#!/usr/bin/env python3
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class SceneEditorMeshPickScrollContractTest(unittest.TestCase):
    def test_geometry_pick_precedes_origin_fallback_for_click_and_hover(self):
        chrome = (ROOT / "src/editor/scene_editor_chrome_actions.c").read_text()
        overlay = (ROOT / "src/editor/scene_editor_digest_overlay.c").read_text()
        for source in (chrome, overlay):
            geometry = source.find("SceneEditorMeshPreviewPickObjectIndex")
            origin = source.find("SceneEditorDigestOverlayPickObjectIndex", geometry)
            self.assertGreaterEqual(geometry, 0)
            self.assertGreater(origin, geometry)

    def test_object_list_uses_shared_scroll_contract_and_clipped_hit_rows(self):
        object_list = (ROOT / "src/editor/scene_editor_object_list.c").read_text()
        self.assertIn("kit_ui_eval_scroll", object_list)
        self.assertIn("kit_ui_scroll_content_height_top_anchor", object_list)
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
