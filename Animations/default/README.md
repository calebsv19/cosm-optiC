# Animations › default

This folder is the active frame dump used by deep render mode. Each BMP (`frame_0000.bmp` through `frame_0054.bmp`) captures one frame rendered by `src/app/animation.c::SaveFrame`, using the resolution from `sceneSettings.windowWidth/Height`.

The `src/tools/make_video.c::MakeVideo` helper renumbers the files sequentially and feeds them to FFmpeg (`ffmpeg -start_number 0 -framerate <fps> -i frame_%04d.bmp …`) when `animSettings.autoMP4` is enabled.
