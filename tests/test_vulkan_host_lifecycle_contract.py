#!/usr/bin/env python3
"""Lock Vulkan host ownership and resize routing at optiC's host edges."""

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


def main() -> None:
    preview = (ROOT / "src/app/preview_session.c").read_text(encoding="utf-8")
    editor = (ROOT / "src/editor/scene_editor.c").read_text(encoding="utf-8")
    editor_header = (ROOT / "include/editor/scene_editor.h").read_text(encoding="utf-8")
    animation = (ROOT / "src/app/animation.c").read_text(encoding="utf-8")

    preview_sync = function_body(preview, "static void PreviewSessionSyncWindowSize(")
    assert "SDL_GetWindowSize(preview_window, &width, &height);" in preview_sync
    assert "PreviewWorkspaceResize(preview_workspace, width, height)" in preview_sync
    preview_host = function_body(preview, "static void RunPreviewInternal(")
    assert "owns_shared_device = (vk_shared_device_get() == NULL);" in preview_host
    assert "SDL_WINDOWEVENT_SIZE_CHANGED" in preview_host
    assert "SDL_WINDOWEVENT_RESIZED" in preview_host
    assert "if (owns_shared_device) {\n            vk_shared_device_shutdown();" in preview_host

    assert "bool owns_shared_device;" in editor_header
    initialize_editor = function_body(editor, "bool InitializeSceneEditor(")
    assert "editor->owns_shared_device = (vk_shared_device_get() == NULL);" in initialize_editor
    assert initialize_editor.count("vk_shared_device_shutdown();") >= 3
    destroy_editor = function_body(editor, "void DestroySceneEditor(")
    assert "if (editor->owns_shared_device)" in destroy_editor
    assert "vk_shared_device_shutdown();" in destroy_editor

    animation_sync = function_body(animation, "static void AnimationSyncWindowSize(")
    assert "SDL_GetWindowSize(window, &width, &height);" in animation_sync
    assert "sceneSettings.windowWidth = width;" in animation_sync
    render_frame = function_body(animation, "void RenderFrame(")
    assert render_frame.find("AnimationSyncWindowSize();") < render_frame.find("setRenderContext(")

    print("Vulkan host lifecycle contract: PASS")


if __name__ == "__main__":
    main()
