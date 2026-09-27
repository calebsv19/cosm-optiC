#ifndef SCENE_EDITOR_VIEWPORT_AUTHORING_BOUNDS_H
#define SCENE_EDITOR_VIEWPORT_AUTHORING_BOUNDS_H

/* Expand existing scene bounds to include camera/light authoring controls.
 * Presentation only: does not change paths, selection or document history. */
void SceneEditorViewportExpandAuthoringBounds(double padding,
    double minimum[3], double maximum[3]);

#endif
