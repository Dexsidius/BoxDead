"""Model and render BoxDead's bullet sprite in Blender.

Run headless (does not touch any open Blender session):

    blender --background --factory-startup --python tools/blender_bullet.py

Renders to tools/_bullet_render.png with a transparent background;
tools/build_bullet.py then downsamples it, adds the dark rim that makes it
readable on light stone, and writes assets/sprites/bullet.bmp.

The round points along +X, matching the gun sprites' "muzzle points right"
convention, so the game can rotate it straight to the velocity angle.
"""
import math
import os
import sys

import bpy

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   '_bullet_render.png')


def clear_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def material(name, base, metallic, roughness, emission=None, strength=0.0):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes['Principled BSDF']
    bsdf.inputs['Base Color'].default_value = (*base, 1.0)
    bsdf.inputs['Metallic'].default_value = metallic
    bsdf.inputs['Roughness'].default_value = roughness
    if emission is not None:
        # Socket names moved around between Blender versions; set whichever
        # this build exposes rather than assuming one.
        for key in ('Emission Color', 'Emission'):
            if key in bsdf.inputs:
                bsdf.inputs[key].default_value = (*emission, 1.0)
                break
        if 'Emission Strength' in bsdf.inputs:
            bsdf.inputs['Emission Strength'].default_value = strength
    return mat


def build_bullet():
    """A jacketed round: cylindrical case, ogive nose, hot tracer tip."""
    parts = []

    # Case. Cylinders are created along +Z, so rotate to lie along +X.
    bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=0.42, depth=1.15,
                                        location=(-0.30, 0, 0),
                                        rotation=(0, math.radians(90), 0))
    case = bpy.context.object
    case.name = 'BulletCase'
    case.data.materials.append(
        material('Brass', (0.62, 0.44, 0.13), 1.0, 0.30))
    parts.append(case)

    # Ogive nose: a cone whose profile is smoothed, so the silhouette curves
    # into the tip rather than being a hard triangle.
    bpy.ops.mesh.primitive_cone_add(vertices=48, radius1=0.42, radius2=0.10,
                                    depth=0.95, location=(0.75, 0, 0),
                                    rotation=(0, math.radians(90), 0))
    nose = bpy.context.object
    nose.name = 'BulletNose'
    nose.data.materials.append(
        material('Jacket', (0.78, 0.60, 0.22), 1.0, 0.22))
    bpy.ops.object.shade_smooth()
    parts.append(nose)

    # Tracer tip: the bright bit that actually reads at 20 pixels.
    bpy.ops.mesh.primitive_uv_sphere_add(segments=24, ring_count=12,
                                         radius=0.16, location=(1.20, 0, 0))
    tip = bpy.context.object
    tip.name = 'BulletTip'
    tip.data.materials.append(
        material('Tracer', (1.0, 0.92, 0.55), 0.0, 0.4,
                 emission=(1.0, 0.88, 0.45), strength=12.0))
    bpy.ops.object.shade_smooth()
    parts.append(tip)

    # Case rim, so the back end does not read as a flat stub.
    bpy.ops.mesh.primitive_torus_add(major_radius=0.40, minor_radius=0.055,
                                     major_segments=40, minor_segments=10,
                                     location=(-0.86, 0, 0),
                                     rotation=(0, math.radians(90), 0))
    rim = bpy.context.object
    rim.name = 'BulletRim'
    rim.data.materials.append(
        material('BrassDark', (0.42, 0.29, 0.09), 1.0, 0.45))
    parts.append(rim)
    return parts


def setup_lighting():
    # Key from the upper left, matching how the iso characters are lit, plus a
    # cool fill so the metal's shadow side does not go black.
    bpy.ops.object.light_add(type='AREA', location=(-2.2, -2.2, 4.0))
    key = bpy.context.object
    key.data.energy = 900.0
    key.data.size = 3.0
    key.rotation_euler = (math.radians(28), math.radians(-22), 0)

    bpy.ops.object.light_add(type='AREA', location=(2.6, 2.4, 2.6))
    fill = bpy.context.object
    fill.data.energy = 260.0
    fill.data.size = 4.0
    fill.data.color = (0.65, 0.75, 1.0)
    fill.rotation_euler = (math.radians(-35), math.radians(30), 0)

    # A dim world keeps the metal reflecting something instead of reading black.
    world = bpy.data.worlds.new('BulletWorld')
    world.use_nodes = True
    world.node_tree.nodes['Background'].inputs[0].default_value = (
        0.22, 0.23, 0.27, 1.0)
    world.node_tree.nodes['Background'].inputs[1].default_value = 1.0
    bpy.context.scene.world = world


def setup_camera():
    # Straight down, orthographic: the sprite is a clean top-down silhouette
    # with no perspective foreshortening along its length.
    bpy.ops.object.camera_add(location=(0.10, 0, 6.0))
    cam = bpy.context.object
    cam.data.type = 'ORTHO'
    cam.data.ortho_scale = 2.9
    cam.rotation_euler = (0, 0, 0)
    bpy.context.scene.camera = cam


def setup_render():
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.device = 'CPU'          # background mode: no GPU context
    scene.cycles.samples = 96
    scene.cycles.use_denoising = True
    scene.render.resolution_x = 640
    scene.render.resolution_y = 320
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = True  # alpha, so the sprite composites
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGBA'
    scene.render.filepath = OUT


def main():
    clear_scene()
    build_bullet()
    setup_lighting()
    setup_camera()
    setup_render()
    bpy.ops.render.render(write_still=True)
    print('BULLET_RENDER_OK ' + OUT)


if __name__ == '__main__':
    main()
