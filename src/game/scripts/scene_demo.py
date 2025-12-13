"""
egen Scene Scripting Demo

This script demonstrates Python scripting capabilities for the egen scene.
It creates a dynamic scene with animated models, color changes, and movement.
"""

import math
import egen_scene as scene

# Global state for the demo
demo_initialized = False
start_time = 0.0
model_indices = []


def init():
    """Initialize the demo scene"""
    global demo_initialized, start_time, model_indices
    
    if demo_initialized:
        return
    
    scene.log("=== Python Scene Scripting Demo ===")
    scene.log("Initializing demo scene...")
    
    start_time = scene.get_time()
    
    # Clear existing models (optional - comment out to keep existing scene)
    # count = scene.get_model_count()
    # for i in range(count - 1, -1, -1):
    #     scene.remove_model(i)
    
    # Add some models to the scene
    scene.log("Adding models...")
    
    # Duck at origin
    scene.add_model("assets/models/duck.glb", (0, 0, 0), 1.0)
    duck_idx = scene.get_model_count() - 1
    scene.enable_model_animation(duck_idx, True, 30.0)
    scene.set_model_color_tint(duck_idx, (1.0, 1.0, 0.8))  # Slight yellow tint
    model_indices.append(duck_idx)
    scene.log(f"Added duck at index {duck_idx}")
    
    # Avocado to the right
    scene.add_model("assets/models/avocado.glb", (3, 0, 0), 0.1)
    avo_idx = scene.get_model_count() - 1
    scene.enable_model_hover(avo_idx, True, 2.0, 0.3)
    scene.set_model_color_tint(avo_idx, (0.8, 1.0, 0.8))  # Green tint
    model_indices.append(avo_idx)
    scene.log(f"Added avocado at index {avo_idx}")
    
    # Another duck to the left, moving back and forth
    scene.add_model("assets/models/duck.glb", (-3, 0, 0), 0.8)
    duck2_idx = scene.get_model_count() - 1
    scene.enable_model_movement(duck2_idx, True, (-3, 0, 0), (-3, 0, -5), 0.8)
    scene.set_model_color_tint(duck2_idx, (0.8, 0.8, 1.0))  # Blue tint
    model_indices.append(duck2_idx)
    scene.log(f"Added moving duck at index {duck2_idx}")
    
    # Another avocado behind, rotating and hovering
    scene.add_model("assets/models/avocado.glb", (0, 0, -4), 0.15)
    avo2_idx = scene.get_model_count() - 1
    scene.enable_model_animation(avo2_idx, True, 45.0)
    scene.enable_model_hover(avo2_idx, True, 1.5, 0.4)
    scene.set_model_color_tint(avo2_idx, (1.0, 0.8, 0.8))  # Red tint
    model_indices.append(avo2_idx)
    scene.log(f"Added animated avocado at index {avo2_idx}")
    
    demo_initialized = True
    scene.log(f"Demo initialized with {len(model_indices)} models")
    scene.log("Press Space to toggle auto-rotate, Tab for wireframe")


def update(elapsed, delta):
    """Update function called each frame"""
    global start_time, model_indices
    
    if not demo_initialized:
        init()
        return
    
    # Get current time relative to start
    t = elapsed - start_time
    
    # Example: Change colors based on time (rainbow effect on first model)
    if len(model_indices) > 0:
        duck_idx = model_indices[0]
        # Create a rainbow color effect
        r = (math.sin(t * 0.5) + 1.0) * 0.5
        g = (math.sin(t * 0.5 + 2.0) + 1.0) * 0.5
        b = (math.sin(t * 0.5 + 4.0) + 1.0) * 0.5
        # Keep it bright
        r = 0.5 + r * 0.5
        g = 0.5 + g * 0.5
        b = 0.5 + b * 0.5
        scene.set_model_color_tint(duck_idx, (r, g, b))
    
    # Example: Rotate second model in a circle
    if len(model_indices) > 1:
        avo_idx = model_indices[1]
        radius = 4.0
        x = math.cos(t * 0.3) * radius
        z = math.sin(t * 0.3) * radius
        scene.set_model_position(avo_idx, (x, 0, z))
        # Rotate to face direction of movement
        angle = math.degrees(t * 0.3) + 90.0
        scene.set_model_rotation(avo_idx, (0, angle, 0))
    
    # Example: Scale pulsing on third model
    if len(model_indices) > 2:
        duck2_idx = model_indices[2]
        scale_factor = 0.8 + math.sin(t * 2.0) * 0.2
        scene.set_model_scale(duck2_idx, (scale_factor, scale_factor, scale_factor))
    
    # Log FPS every 5 seconds
    if int(t) % 5 == 0 and int(t * 60) % 300 == 0:  # Roughly every 5 seconds
        fps = scene.get_fps()
        scene.log(f"FPS: {fps:.1f}, Models: {scene.get_model_count()}")


# Auto-initialize when script is loaded
init()

# Export update function for the engine to call
__all__ = ['init', 'update']

