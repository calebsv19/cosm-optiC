#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>
#include "editor/scene_editor.h"
#include "app/animation.h"  // Include the header where RunMainLoop() is declared
#include "config/config_manager.h"

// Window & Menu Layout
#define MENU_WIDTH 800
#define MENU_HEIGHT 600
#define MENU_MARGIN_X 30   // Distance from left edge
#define MENU_MARGIN_Y 30   // Distance from top

// Main Toggle Buttons (Interactive vs. Deep Render)
#define TOGGLE_BUTTON_WIDTH 200
#define TOGGLE_BUTTON_HEIGHT 50
#define TOGGLE_BUTTON_MARGIN_X MENU_MARGIN_X + 0
#define TOGGLE_BUTTON_MARGIN_Y MENU_MARGIN_Y + 0
#define TOGGLE_BUTTON_TEXT_SIZE 24
#define TOGGLE_BUTTON_SPACING 10

// Deep Render Sub-Settings Buttons
#define SUBSETTING_BUTTON_WIDTH 175
#define SUBSETTING_BUTTON_HEIGHT 40
#define SUBSETTING_BUTTON_MARGIN_X (MENU_MARGIN_X + 10)  // Nested under Deep Render
#define SUBSETTING_BUTTON_MARGIN_Y TOGGLE_BUTTON_MARGIN_Y + (TOGGLE_BUTTON_HEIGHT + TOGGLE_BUTTON_SPACING) * 2
#define SUBSETTING_TEXT_SIZE 22
#define SUBSETTING_BUTTON_SPACING 10

// Editable Number Fields (Bounce Limit & Frame Limit)
#define VALUE_BOX_WIDTH 50
#define VALUE_BOX_HEIGHT 50
#define VALUE_BOX_MARGIN_X 20  // Right-aligned to corresponding button
#define VALUE_BOX_MARGIN_Y 10   // Slightly below the top of button
#define VALUE_BOX_TEXT_SIZE 20

// Bottom-Right Buttons (Start & Restore Defaults)
#define BOTTOM_BUTTON_SPACING 10

#define BOTTOM_BUTTON_WIDTH_START 200
#define BOTTOM_BUTTON_HEIGHT_START 50
#define BOTTOM_BUTTON_MARGIN_X_START (MENU_WIDTH - MENU_MARGIN_X - BOTTOM_BUTTON_WIDTH_START)
#define BOTTOM_BUTTON_MARGIN_Y_START (MENU_HEIGHT -MENU_MARGIN_Y - BOTTOM_BUTTON_HEIGHT_START - 0)

#define BOTTOM_BUTTON_WIDTH_EXIT 180
#define BOTTOM_BUTTON_HEIGHT_EXIT 40 
#define BOTTOM_BUTTON_MARGIN_X_EXIT (MENU_MARGIN_X)
#define BOTTOM_BUTTON_MARGIN_Y_EXIT (MENU_HEIGHT -MENU_MARGIN_Y - BOTTOM_BUTTON_HEIGHT_RESTORE - 0)

#define BOTTOM_BUTTON_WIDTH_RESTORE 180
#define BOTTOM_BUTTON_HEIGHT_RESTORE 40
#define BOTTOM_BUTTON_MARGIN_X_RESTORE (BOTTOM_BUTTON_MARGIN_X_EXIT + BOTTOM_BUTTON_WIDTH_RESTORE + 10)
#define BOTTOM_BUTTON_MARGIN_Y_RESTORE (MENU_HEIGHT -MENU_MARGIN_Y - BOTTOM_BUTTON_HEIGHT_RESTORE - 0)

#define BOTTOM_BUTTON_WIDTH_SAVE 100
#define BOTTOM_BUTTON_HEIGHT_SAVE 40
#define BOTTOM_BUTTON_MARGIN_X_SAVE (BOTTOM_BUTTON_MARGIN_X_RESTORE + BOTTOM_BUTTON_WIDTH_RESTORE + 10)
#define BOTTOM_BUTTON_MARGIN_Y_SAVE (MENU_HEIGHT -MENU_MARGIN_Y - BOTTOM_BUTTON_HEIGHT_RESTORE - 0)

// Slider Layout Constants
#define SLIDER_WIDTH 250
#define SLIDER_HEIGHT 10
#define SLIDER_SPACING 20  // Vertical spacing between sliders

// Align all sliders to the top-right of the menu
#define SLIDER_MARGIN_X (MENU_WIDTH - SLIDER_WIDTH - MENU_MARGIN_X - 40)
#define SLIDER_MARGIN_Y MENU_MARGIN_Y  // Align with top of window

// Individual Slider Positions (stacked vertically)
#define SLIDER_BOUNCE_X SLIDER_MARGIN_X
#define SLIDER_BOUNCE_Y SLIDER_MARGIN_Y

#define SLIDER_FRAME_X SLIDER_MARGIN_X
#define SLIDER_FRAME_Y (SLIDER_BOUNCE_Y + SLIDER_HEIGHT + SLIDER_SPACING)

#define SLIDER_TRAVEL_X SLIDER_MARGIN_X
#define SLIDER_TRAVEL_Y (SLIDER_FRAME_Y + SLIDER_HEIGHT + SLIDER_SPACING)

#define SLIDER_FPS_X SLIDER_MARGIN_X
#define SLIDER_FPS_Y (SLIDER_TRAVEL_Y + SLIDER_HEIGHT + SLIDER_SPACING)

#define SLIDER_LIGHT_X SLIDER_MARGIN_X
#define SLIDER_LIGHT_Y (SLIDER_FPS_Y + SLIDER_HEIGHT + SLIDER_SPACING)

#define SLIDER_RAYS_X SLIDER_MARGIN_X
#define SLIDER_RAYS_Y (SLIDER_LIGHT_Y + SLIDER_HEIGHT + SLIDER_SPACING)

#define SLIDER_WIDTH_X SLIDER_MARGIN_X
#define SLIDER_WIDTH_Y (SLIDER_RAYS_Y + SLIDER_HEIGHT + SLIDER_SPACING)

#define SLIDER_HEIGHT_X SLIDER_MARGIN_X
#define SLIDER_HEIGHT_Y (SLIDER_WIDTH_Y + SLIDER_HEIGHT + SLIDER_SPACING)


// Default settings
#define DEFAULT_BOUNCE_LIMIT 10
#define DEFAULT_FRAME_LIMIT 50
#define DEFAULT_FRAME_FOR_TRAVEL 40

// Global variables for slider interaction
bool draggingSlider = false;  // Tracks whether a slider is being dragged
int *selectedSlider = NULL;   // Pointer to the currently selected slider value
int selectedSliderMin = 0;    // Min value of the selected slider
int selectedSliderMax = 100;  // Max value of the selected slider
int sliderStartX = 0;         // X position of the slider bar
int sliderWidth = 0;          // Width of the slider
char inputBuffer[10] = "";   // Stores user-typed numbers
bool editingBounce = false;  // Tracks if bounce limit is being edited
bool editingFrame = false;   // Tracks if frame limit is being edited

bool InitializeMenu(SDL_Window** window, SDL_Renderer** renderer, TTF_Font** font) {
    // Initialize SDL video subsystem
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL Initialization Failed: %s\n", SDL_GetError());
        return false;
    }

    // Initialize SDL_ttf for text rendering
    if (TTF_Init() == -1) {
        printf("TTF Initialization Failed: %s\n", TTF_GetError());
        SDL_Quit();
        return false;
    }

    // Create SDL Window
    *window = SDL_CreateWindow("RayTracing Menu",
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               MENU_WIDTH, MENU_HEIGHT, SDL_WINDOW_SHOWN);
    if (!*window) {
        printf("Window Creation Failed: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    // Create SDL Renderer
    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED);
    if (!*renderer) {
        printf("Renderer Creation Failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(*window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    // Load Font
    *font = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial.ttf", TOGGLE_BUTTON_TEXT_SIZE);
    if (!*font) {
        printf("Font Loading Failed: %s\n", TTF_GetError());
        SDL_DestroyRenderer(*renderer);
        SDL_DestroyWindow(*window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    // Load animation settings
    LoadAnimationConfig();
    LoadSceneConfig();
    return true;  // Menu initialized successfully
}


void ResetAnimationSettings(void) {
    animSettings.interactiveMode = true;
    animSettings.deepRenderMode = false;
    animSettings.bounceMode = false;
    animSettings.autoMP4 = false;
    animSettings.bounceLimit = DEFAULT_BOUNCE_LIMIT;
    animSettings.frameLimit = DEFAULT_FRAME_LIMIT;
    animSettings.framesForTravel = DEFAULT_FRAME_FOR_TRAVEL;
    animSettings.fps = 30;
}

                     
void RenderText(SDL_Renderer *renderer, TTF_Font *font, int x, int y, const char *format, ...) {
    char buffer[32];  // Store formatted text
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    SDL_Color textColor = {255, 255, 255, 255};
    SDL_Surface *textSurface = TTF_RenderText_Solid(font, buffer, textColor);
    SDL_Texture *textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
                        
    SDL_Rect textRect = {x, y, textSurface->w, textSurface->h};
    SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
                 
    SDL_FreeSurface(textSurface);
    SDL_DestroyTexture(textTexture);
}
            

void RenderButton(SDL_Renderer *renderer, TTF_Font *font, int x, int y, int width, int height, const char *text, bool active) {
    SDL_Rect rect = {x, y, width, height};

    // Toggle button color based on active state
    SDL_SetRenderDrawColor(renderer, active ? 0 : 100, active ? 255 : 100, active ? 0 : 100, 255);
    SDL_RenderFillRect(renderer, &rect);

    // Render text centered inside the button
    SDL_Color textColor = {255, 255, 255, 255};
    SDL_Surface *textSurface = TTF_RenderText_Solid(font, text, textColor);
    SDL_Texture *textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);

    int textX = x + (width - textSurface->w) / 2;  // Center text horizontally
    int textY = y + (height - textSurface->h) / 2; // Center text vertically

    SDL_Rect textRect = {textX, textY, textSurface->w, textSurface->h};
    SDL_RenderCopy(renderer, textTexture, NULL, &textRect);

    SDL_FreeSurface(textSurface);
    SDL_DestroyTexture(textTexture);
}
void RenderSliders(SDL_Renderer* renderer, TTF_Font* font) {
    struct Slider {
        int *value;
        int min, max, x, y, width;
        const char *label;
    };

    struct Slider sliders[] = {
        { &animSettings.bounceLimit, 0, 100, SLIDER_BOUNCE_X, SLIDER_BOUNCE_Y, SLIDER_WIDTH, "Bounce Limit" },
        { &animSettings.frameLimit, 1, 5000, SLIDER_FRAME_X, SLIDER_FRAME_Y, SLIDER_WIDTH, "Frame Limit" },
        { &animSettings.framesForTravel, 1, 5000, SLIDER_TRAVEL_X, SLIDER_TRAVEL_Y, SLIDER_WIDTH, "Path Points" },
        { &animSettings.fps, 1, 240, SLIDER_FPS_X, SLIDER_FPS_Y, SLIDER_WIDTH, "FPS" },
        { &animSettings.lightMode, 0, 1, SLIDER_LIGHT_X, SLIDER_LIGHT_Y, SLIDER_WIDTH, "Light Mode" },
	{ &sceneSettings.rays, 0, 10000, SLIDER_RAYS_X, SLIDER_RAYS_Y, SLIDER_WIDTH, "Num Rays" },
 	{ &sceneSettings.windowWidth, 0, 2500, SLIDER_WIDTH_X, SLIDER_WIDTH_Y, SLIDER_WIDTH, "Width" },
	{ &sceneSettings.windowHeight, 0, 1400, SLIDER_HEIGHT_X, SLIDER_HEIGHT_Y, SLIDER_WIDTH, "Height" },
    };

    size_t sliderCount = sizeof(sliders) / sizeof(sliders[0]);
    for (size_t i = 0; i < sliderCount; i++) {
        int textMarginX = 5;
        // Render label to the left of the slider
        RenderText(renderer, font, sliders[i].x - 150, sliders[i].y - textMarginX, "%s", sliders[i].label);

        // Render slider background
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        SDL_Rect sliderBar = {sliders[i].x, sliders[i].y, sliders[i].width, 10};
        SDL_RenderFillRect(renderer, &sliderBar);

        // Calculate knob position based on value range
        int knobX = sliders[i].x + ((float)(*sliders[i].value - sliders[i].min) / (sliders[i].max - sliders[i].min)) 
					* sliders[i].width;

        // Render draggable knob
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_Rect knob = {knobX - 5, sliders[i].y - 5, 10, 20};
        SDL_RenderFillRect(renderer, &knob);

        // Render value next to the slider
        RenderText(renderer, font, sliders[i].x + sliders[i].width + 10, sliders[i].y- textMarginX,
 			"%d", *sliders[i].value);
    }
}

void RenderMenu(SDL_Renderer* renderer, TTF_Font* font) {  
    // Clear the screen with a black background
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    
    // Render main mode buttons
    RenderButton(renderer, font, TOGGLE_BUTTON_MARGIN_X, TOGGLE_BUTTON_MARGIN_Y,
                 TOGGLE_BUTTON_WIDTH, TOGGLE_BUTTON_HEIGHT, "Interactive Mode", animSettings.interactiveMode);
    
    RenderButton(renderer, font, TOGGLE_BUTTON_MARGIN_X,
                 TOGGLE_BUTTON_MARGIN_Y + (TOGGLE_BUTTON_HEIGHT + TOGGLE_BUTTON_SPACING),
                 TOGGLE_BUTTON_WIDTH, TOGGLE_BUTTON_HEIGHT, "Deep Render", animSettings.deepRenderMode);
    
    // Render Deep Render settings when enabled
    if (animSettings.deepRenderMode) {
        RenderButton(renderer, font, SUBSETTING_BUTTON_MARGIN_X, SUBSETTING_BUTTON_MARGIN_Y,
                     SUBSETTING_BUTTON_WIDTH, SUBSETTING_BUTTON_HEIGHT, "Bounce Mode", animSettings.bounceMode);
    
        RenderButton(renderer, font, SUBSETTING_BUTTON_MARGIN_X,
                     SUBSETTING_BUTTON_MARGIN_Y + (SUBSETTING_BUTTON_SPACING + SUBSETTING_BUTTON_HEIGHT),
                     SUBSETTING_BUTTON_WIDTH, SUBSETTING_BUTTON_HEIGHT, "Auto MP4", animSettings.autoMP4);
        
        RenderButton(renderer, font, SUBSETTING_BUTTON_MARGIN_X,
                     SUBSETTING_BUTTON_MARGIN_Y + 2 * (SUBSETTING_BUTTON_SPACING + SUBSETTING_BUTTON_HEIGHT),
                     SUBSETTING_BUTTON_WIDTH, SUBSETTING_BUTTON_HEIGHT, "Scene Editor", false);
	// Render Scene Editor Mode Toggle Button
	const char* editorModeText = (animSettings.editorMode == 0) ? "Path" :
                             (animSettings.editorMode == 1) ? "Scene" : "Camera";

	RenderButton(renderer, font, SUBSETTING_BUTTON_MARGIN_X,
        	     SUBSETTING_BUTTON_MARGIN_Y + 3 * (SUBSETTING_BUTTON_SPACING + SUBSETTING_BUTTON_HEIGHT),
             	     SUBSETTING_BUTTON_WIDTH, SUBSETTING_BUTTON_HEIGHT, editorModeText, false);
 	/*   
        // Render Editable Number Fields (Bounce Limit & Frame Limit)
        if (editingBounce) {
            RenderText(renderer, font, VALUE_BOX_MARGIN_X + SUBSETTING_BUTTON_WIDTH + SUBSETTING_BUTTON_MARGIN_X,
                       TOGGLE_BUTTON_MARGIN_Y + 2 * (TOGGLE_BUTTON_HEIGHT + TOGGLE_BUTTON_SPACING) + VALUE_BOX_MARGIN_Y,
                       "%s_", inputBuffer);  // Show cursor
        } else {
            RenderText(renderer, font, VALUE_BOX_MARGIN_X + SUBSETTING_BUTTON_WIDTH + SUBSETTING_BUTTON_MARGIN_X,
                       TOGGLE_BUTTON_MARGIN_Y + 2 * (TOGGLE_BUTTON_HEIGHT + TOGGLE_BUTTON_SPACING) + VALUE_BOX_MARGIN_Y,
                       "%d", animSettings.bounceLimit);
        }
                
        if (editingFrame) {
            RenderText(renderer, font, VALUE_BOX_MARGIN_X + TOGGLE_BUTTON_WIDTH + TOGGLE_BUTTON_MARGIN_X,
                       TOGGLE_BUTTON_MARGIN_Y + (TOGGLE_BUTTON_HEIGHT + TOGGLE_BUTTON_SPACING) + VALUE_BOX_MARGIN_Y,
                       "%s_", inputBuffer);
        } else {
            RenderText(renderer, font, VALUE_BOX_MARGIN_X + TOGGLE_BUTTON_WIDTH + TOGGLE_BUTTON_MARGIN_X,
                       TOGGLE_BUTTON_MARGIN_Y + (TOGGLE_BUTTON_HEIGHT + TOGGLE_BUTTON_SPACING) + VALUE_BOX_MARGIN_Y,
                       "%d", animSettings.framesForTravel);
        }
	*/
    }
                 
    // Render Bottom Buttons
    RenderButton(renderer, font, BOTTOM_BUTTON_MARGIN_X_SAVE, BOTTOM_BUTTON_MARGIN_Y_SAVE,
                 BOTTOM_BUTTON_WIDTH_SAVE, BOTTOM_BUTTON_HEIGHT_SAVE, "Save", false);
    RenderButton(renderer, font, BOTTOM_BUTTON_MARGIN_X_RESTORE, BOTTOM_BUTTON_MARGIN_Y_RESTORE,
                 BOTTOM_BUTTON_WIDTH_RESTORE, BOTTOM_BUTTON_HEIGHT_RESTORE, "Restore Defaults", false);
    RenderButton(renderer, font, BOTTOM_BUTTON_MARGIN_X_EXIT, BOTTOM_BUTTON_MARGIN_Y_EXIT,
                 BOTTOM_BUTTON_WIDTH_EXIT, BOTTOM_BUTTON_HEIGHT_EXIT, "Exit w/o Saving", false); 
    RenderButton(renderer, font, BOTTOM_BUTTON_MARGIN_X_START, BOTTOM_BUTTON_MARGIN_Y_START,
                 BOTTOM_BUTTON_WIDTH_START, BOTTOM_BUTTON_HEIGHT_START, "Start", false);
   

    RenderSliders(renderer, font);

    // Present the updated UI 
    SDL_RenderPresent(renderer);
} 

void HandleKeyPress(SDL_Event* event, bool* running) {
    switch (event->key.keysym.sym) {
        case SDLK_BACKSPACE:
            if ((editingBounce || editingFrame) && strlen(inputBuffer) > 0) {
                inputBuffer[strlen(inputBuffer) - 1] = '\0';
            }
            break;
        case SDLK_RETURN:
            if (strlen(inputBuffer) > 0) {
                int newValue = atoi(inputBuffer);
                if (editingBounce) animSettings.bounceLimit = newValue;
                if (editingFrame) animSettings.frameLimit = newValue;
            }
            editingBounce = false;
            editingFrame = false;
            inputBuffer[0] = '\0'; 
            break;
        case SDLK_ESCAPE:
            *running = false;
            break;
        case SDLK_i:
            animSettings.interactiveMode = true;
            animSettings.deepRenderMode = false;
            break;
        case SDLK_d:
            animSettings.deepRenderMode = true;
            animSettings.interactiveMode = false;
            break;
        case SDLK_b:
            if (animSettings.deepRenderMode) animSettings.bounceMode = !animSettings.bounceMode;
            break;
        case SDLK_m:
            if (animSettings.deepRenderMode) animSettings.autoMP4 = !animSettings.autoMP4;
            break;
        case SDLK_p:
            if (animSettings.deepRenderMode){
		// ✅ Initialize the Scene Editor and Run the Loop
                SceneEditor editor = {0};  // Zero-initialize struct
                InitializeSceneEditor(&editor);
                editor.running = true;
                SceneEditorLoop(&editor);
                *running = false;  // Exit menu after scene starts
	    }
            break;
        case SDLK_r:
            animSettings.interactiveMode = true;
            animSettings.deepRenderMode = false;
            animSettings.bounceMode = false;
            animSettings.autoMP4 = false;
            animSettings.bounceLimit = DEFAULT_BOUNCE_LIMIT;
            animSettings.frameLimit = DEFAULT_FRAME_LIMIT;
            animSettings.framesForTravel = DEFAULT_FRAME_FOR_TRAVEL;
            break;
    }
}

void HandleMouseMotion(SDL_Event* event) {
    if (!draggingSlider || !selectedSlider) return;  // Ensure a slider is being dragged

    int x = event->motion.x;

    // Calculate new slider value based on mouse position
    float percent = (float)(x - sliderStartX) / sliderWidth;
    int newValue = selectedSliderMin + percent * (selectedSliderMax - selectedSliderMin);

    // Clamp value within range
    if (newValue < selectedSliderMin) newValue = selectedSliderMin;
    if (newValue > selectedSliderMax) newValue = selectedSliderMax;

    *selectedSlider = newValue;  // Update the selected value
}

void HandleSliderClick(SDL_Event* event) {
    int x = event->button.x, y = event->button.y;
    
    // Define sliders with their values, ranges, and positions
    struct Slider {
        int *value;
        int min, max, x, y, width;
    };
     
    struct Slider sliders[] = {
        { &animSettings.bounceLimit, 0, 100, SLIDER_BOUNCE_X, SLIDER_BOUNCE_Y, SLIDER_WIDTH },
        { &animSettings.frameLimit, 1, 5000, SLIDER_FRAME_X, SLIDER_FRAME_Y, SLIDER_WIDTH },
        { &animSettings.framesForTravel, 1, 5000, SLIDER_TRAVEL_X, SLIDER_TRAVEL_Y, SLIDER_WIDTH },
        { &animSettings.fps, 1, 240, SLIDER_FPS_X, SLIDER_FPS_Y, SLIDER_WIDTH },
        { &animSettings.lightMode, 0, 1, SLIDER_LIGHT_X, SLIDER_LIGHT_Y, SLIDER_WIDTH },
	{ &sceneSettings.rays, 0, 10000, SLIDER_RAYS_X, SLIDER_RAYS_Y, SLIDER_WIDTH},
	{ &sceneSettings.windowWidth, 0, 2500, SLIDER_WIDTH_X, SLIDER_WIDTH_Y, SLIDER_WIDTH},
        { &sceneSettings.windowHeight, 0, 1400, SLIDER_HEIGHT_X, SLIDER_HEIGHT_Y, SLIDER_WIDTH}
    };
         
    size_t sliderCount = sizeof(sliders) / sizeof(sliders[0]);
    for (size_t i = 0; i < sliderCount; i++) {
        if (x > sliders[i].x && x < sliders[i].x + sliders[i].width &&
            y > sliders[i].y - 5 && y < sliders[i].y + 15) {
            
            // Activate dragging mode
            draggingSlider = true;
            selectedSlider = sliders[i].value;
            selectedSliderMin = sliders[i].min;
            selectedSliderMax = sliders[i].max;
            sliderStartX = sliders[i].x;
            sliderWidth = sliders[i].width;

            // Immediately update slider value to where the user clicked
            float percent = (float)(x - sliders[i].x) / sliders[i].width;
            int newValue = sliders[i].min + percent * (sliders[i].max - sliders[i].min);

            // Clamp value within range
            if (newValue < sliders[i].min) newValue = sliders[i].min;
            if (newValue > sliders[i].max) newValue = sliders[i].max;

            *selectedSlider = newValue;  // Apply updated value instantly
            return;
        }
    }
}


void HandleMouseClick(SDL_Event* event, bool* running, bool* menuExitedNormally, SDL_Renderer* renderer) {
    (void)renderer;
    HandleSliderClick(event);

    int x = event->button.x, y = event->button.y;
                
    // Toggle Interactive Mode
    if (x > TOGGLE_BUTTON_MARGIN_X && x < TOGGLE_BUTTON_MARGIN_X + TOGGLE_BUTTON_WIDTH &&
        y > TOGGLE_BUTTON_MARGIN_Y && y < TOGGLE_BUTTON_MARGIN_Y + TOGGLE_BUTTON_HEIGHT) {
        animSettings.interactiveMode = true;
        animSettings.deepRenderMode = false;
        return;
    }
        
    // Toggle Deep Render Mode
    if (x > TOGGLE_BUTTON_MARGIN_X && x < TOGGLE_BUTTON_MARGIN_X + TOGGLE_BUTTON_WIDTH &&
        y > TOGGLE_BUTTON_MARGIN_Y + (TOGGLE_BUTTON_HEIGHT + TOGGLE_BUTTON_SPACING) &&
        y < TOGGLE_BUTTON_MARGIN_Y + TOGGLE_BUTTON_HEIGHT + (TOGGLE_BUTTON_HEIGHT + TOGGLE_BUTTON_SPACING)) {
        animSettings.deepRenderMode = true;
        animSettings.interactiveMode = false;
        return;
    }
            
    // Deep Render Mode Options (Only Available When Deep Render is ON)
    if (animSettings.deepRenderMode) {
        if (x > SUBSETTING_BUTTON_MARGIN_X && x < SUBSETTING_BUTTON_MARGIN_X + SUBSETTING_BUTTON_WIDTH &&
            y > SUBSETTING_BUTTON_MARGIN_Y && y < SUBSETTING_BUTTON_MARGIN_Y + SUBSETTING_BUTTON_HEIGHT) {
            animSettings.bounceMode = !animSettings.bounceMode;
            return;
        }
        
        if (x > SUBSETTING_BUTTON_MARGIN_X && x < SUBSETTING_BUTTON_MARGIN_X + SUBSETTING_BUTTON_WIDTH &&
            y > SUBSETTING_BUTTON_MARGIN_Y + (SUBSETTING_BUTTON_HEIGHT + SUBSETTING_BUTTON_SPACING) &&
            y < SUBSETTING_BUTTON_MARGIN_Y + SUBSETTING_BUTTON_HEIGHT + (SUBSETTING_BUTTON_HEIGHT + 
			SUBSETTING_BUTTON_SPACING)) {
            animSettings.autoMP4 = !animSettings.autoMP4;
            return;
        }
            
        if (x > SUBSETTING_BUTTON_MARGIN_X && x < SUBSETTING_BUTTON_MARGIN_X + SUBSETTING_BUTTON_WIDTH &&
            y > SUBSETTING_BUTTON_MARGIN_Y + 2 * (SUBSETTING_BUTTON_HEIGHT + SUBSETTING_BUTTON_SPACING) &&
            y < SUBSETTING_BUTTON_MARGIN_Y + 2 * (SUBSETTING_BUTTON_HEIGHT + SUBSETTING_BUTTON_SPACING) + 
			SUBSETTING_BUTTON_HEIGHT) {
	    // Initialize the Scene Editor and Run the Loop
            SceneEditor editor = {0};  // Zero-initialize struct
            InitializeSceneEditor(&editor);
            editor.running = true;
            SceneEditorLoop(&editor);
            return;
        }

	// Check if clicking on the Scene Editor Mode Toggle Button
        if (x >= SUBSETTING_BUTTON_MARGIN_X &&
            x <= SUBSETTING_BUTTON_MARGIN_X + SUBSETTING_BUTTON_WIDTH &&
            y >= SUBSETTING_BUTTON_MARGIN_Y + 3 * (SUBSETTING_BUTTON_SPACING + SUBSETTING_BUTTON_HEIGHT) &&
            y <= SUBSETTING_BUTTON_MARGIN_Y + 3 * (SUBSETTING_BUTTON_SPACING + SUBSETTING_BUTTON_HEIGHT) + SUBSETTING_BUTTON_HEIGHT) {
        
            //  Cycle through the three modes
            animSettings.editorMode = (animSettings.editorMode + 1) % 3;

            //  Print the new mode
            const char* newModeText = (animSettings.editorMode == 0) ? "Camera" :
                                  (animSettings.editorMode == 1) ? "Scene" : "Path";
            printf("Scene Editor Mode Toggled: %s\n", newModeText);
        }


	/*
        // Click on Bounce Limit Box
        if (x > VALUE_BOX_MARGIN_X + SUBSETTING_BUTTON_WIDTH + SUBSETTING_BUTTON_MARGIN_X &&
            x < VALUE_BOX_MARGIN_X + VALUE_BOX_WIDTH + SUBSETTING_BUTTON_WIDTH + SUBSETTING_BUTTON_MARGIN_X &&
            y > SUBSETTING_BUTTON_MARGIN_Y && y < SUBSETTING_BUTTON_MARGIN_Y + VALUE_BOX_HEIGHT + 
			VALUE_BOX_MARGIN_Y) {
            editingBounce = true; 
            editingFrame = false;
            snprintf(inputBuffer, sizeof(inputBuffer), "%d", animSettings.bounceLimit);
            return;
        }
        
        // Click anywhere else → Save current input
        if (!(editingBounce || editingFrame)) {
            if (strlen(inputBuffer) > 0) {
                int newValue = atoi(inputBuffer);
                if (editingBounce) animSettings.bounceLimit = newValue;
                if (editingFrame) animSettings.framesForTravel = newValue;
            }
            editingBounce = false;
            editingFrame = false;
            inputBuffer[0] = '\0';  // Clear buffer
        } 
	*/       
    } 
    // Restore Defaults Button Click
    if (x > BOTTOM_BUTTON_MARGIN_X_SAVE && x < BOTTOM_BUTTON_MARGIN_X_SAVE + BOTTOM_BUTTON_WIDTH_SAVE &&
        y > BOTTOM_BUTTON_MARGIN_Y_SAVE && y < BOTTOM_BUTTON_MARGIN_Y_SAVE + BOTTOM_BUTTON_HEIGHT_SAVE) {
	SaveAllSettings();
    }

    // Restore Defaults Button Click
    if (x > BOTTOM_BUTTON_MARGIN_X_RESTORE && x < BOTTOM_BUTTON_MARGIN_X_RESTORE + BOTTOM_BUTTON_WIDTH_RESTORE &&
        y > BOTTOM_BUTTON_MARGIN_Y_RESTORE && y < BOTTOM_BUTTON_MARGIN_Y_RESTORE + BOTTOM_BUTTON_HEIGHT_RESTORE) {
	ResetAnimationSettings();
    }
    // Exit without saving Button Click
    if (x > BOTTOM_BUTTON_MARGIN_X_EXIT && x < BOTTOM_BUTTON_MARGIN_X_EXIT + BOTTOM_BUTTON_WIDTH_EXIT &&
        y > BOTTOM_BUTTON_MARGIN_Y_EXIT && y < BOTTOM_BUTTON_MARGIN_Y_EXIT + BOTTOM_BUTTON_HEIGHT_EXIT) {
	*running = false;
    }
    
    // Start Button Click
    if (x > BOTTOM_BUTTON_MARGIN_X_START && x < BOTTOM_BUTTON_MARGIN_X_START + BOTTOM_BUTTON_WIDTH_START &&
        y > BOTTOM_BUTTON_MARGIN_Y_START && y < BOTTOM_BUTTON_MARGIN_Y_START + BOTTOM_BUTTON_HEIGHT_START) {
	SaveAllSettings();
        *menuExitedNormally = true;
        *running = false;
    }
}

bool RunMenu(void) {        
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;
            
    if (!InitializeMenu(&window, &renderer, &font)) {
        return false;
    }   
        
    bool running = true;
    bool menuExitedNormally = false;
    SDL_Event event;
                
    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    menuExitedNormally = false;
                    running = false;
                    break;

                case SDL_KEYDOWN:
                    HandleKeyPress(&event, &running);
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    HandleMouseClick(&event, &running, &menuExitedNormally, renderer);
                    break;

                case SDL_MOUSEBUTTONUP:
                    draggingSlider = false;  // Stop dragging on mouse release
                    break;

                case SDL_MOUSEMOTION:
                    HandleMouseMotion(&event);  // Update slider value on drag
                    break;

                case SDL_TEXTINPUT:
                    if (editingBounce || editingFrame) {
                        if (strlen(inputBuffer) < sizeof(inputBuffer) - 1) {
                            strcat(inputBuffer, event.text.text);
                        }
                    }
                    break;
            }
        }
        RenderMenu(renderer, font);
    }
    //  Only Quit SDL if the User Exits the Menu
    if (menuExitedNormally) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_CloseFont(font);
        TTF_Quit();
        SDL_Quit();
    }
    SaveAllSettings();
    return menuExitedNormally;
}
