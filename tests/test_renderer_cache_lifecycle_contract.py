#!/usr/bin/env python3
"""Lock renderer-cache detach ordering at every relevant optiC teardown edge."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def function_body(source: str, signature: str) -> str:
    start = source.find(signature)
    assert start >= 0, f"missing function: {signature}"
    brace = source.find("{", start)
    assert brace >= 0, f"missing function body: {signature}"
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[brace + 1 : index]
    raise AssertionError(f"unterminated function body: {signature}")


def assert_detach_before_destroy(body: str, renderer_expr: str) -> None:
    detach = f"MaterialEditorFacePreviewDetachRenderer({renderer_expr});"
    detach_index = body.find(detach)
    assert detach_index >= 0, f"missing face-preview detach for {renderer_expr}"
    destroy_indexes = [
        index
        for token in ("vk_renderer_shutdown_surface", "SDL_DestroyRenderer")
        if (index := body.find(token)) >= 0
    ]
    assert destroy_indexes, "missing renderer destruction call"
    assert detach_index < min(destroy_indexes), "face-preview detach must precede renderer destruction"


def main() -> None:
    menu_source = (ROOT / "src/ui/menu/sdl_menu.c").read_text(encoding="utf-8")
    editor_source = (ROOT / "src/editor/scene_editor.c").read_text(encoding="utf-8")

    initialize_menu = function_body(menu_source, "static bool initialize_menu(")
    shutdown_menu = function_body(menu_source, "static void shutdown_menu(")
    session_end = function_body(editor_source, "void SceneEditorSessionEnd(")
    destroy_editor = function_body(editor_source, "void DestroySceneEditor(")

    assert_detach_before_destroy(initialize_menu, "*renderer")
    assert_detach_before_destroy(shutdown_menu, "renderer")
    assert "MaterialEditorFacePreviewDetachRenderer(editor->renderer);" in session_end
    assert_detach_before_destroy(destroy_editor, "editor->renderer")
    print("renderer cache lifecycle contract: PASS")


if __name__ == "__main__":
    main()
