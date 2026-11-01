"""Stamp NieR world roles into object names before FBX export.

Why this exists
---------------
`Nier_Fbx_Rule` classifies meshes by name because a name is all an FBX gives it.
The authoritative role data lives in the `.blend` instead, in two places the FBX
export destroys outright:

  * Collection membership - `WMB` / `g<id>`, `COL`, `LAY`, `lay_layAssets`,
    `lay_layInstances`, `col_colTreeNodes`, `wmb_colTreeNodes`
  * `id_properties` - `mesh_group_props.lod_name` / `.lod_level`, `col_mesh_props`

Measured against the five shipped `.blend` files, names alone recover the role
for only 53%-100% of meshes depending on the map, and two maps are impossible
by name at any effort:

    CityOfRuinEntry      916 meshes  100.0%
    CityOfRuinEntry_LOD   63 meshes  100.0%
    CityOfRuin_LOD       159 meshes  100.0%
    CityOfRuinBridge     648 meshes   94.4%   (36 SHADOW unmarked)
    CityOfRuinManHole   1327 meshes   72.8%   (291 COL unmarked)
    CityOfRuinMainField  781 meshes   52.8%   (335 COL unmarked)

MainField has 246 collision meshes named `GROUND_11220.*` while 23 *visual*
meshes share that exact base name; ManHole collides on three base names. No
naming rule can separate those, so the fix has to happen before export: stamp
the role the `.blend` already knows into the name, then the FBX describes itself
and `Nier_Fbx_Rule` is exact.

Run
---
    blender CityOfRuinMainField.blend --background --python stamp_nier_roles.py

Then export the FBX as usual. `--` arguments:
    --dry-run       report what would change, rename nothing
    --lay-out PATH  write LAY placements to PATH (default: alongside the .blend)

The LAY dump is separate on purpose: placements are Blender Empties, and an FBX
mesh export drops every one of them, so that data cannot reach the converter
through the mesh path at any point.
"""

import json
import os
import sys

import bpy

# Collection name -> stamped prefix. First match in this order wins.
COLLECTION_PREFIX = (
    ("COL", "COL"),
    ("LAY", "INST"),
    ("lay_layAssets", "INST"),
    ("lay_layInstances", "INST"),
)

# Spatial-tree Empties. Excluded from the mesh export; they are structure, not geometry.
TREE_COLLECTIONS = ("col_colTreeNodes", "wmb_colTreeNodes")

LAY_COLLECTIONS = ("LAY", "lay_layAssets", "lay_layInstances")


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    dry_run = "--dry-run" in argv
    lay_out = None
    if "--lay-out" in argv:
        lay_out = argv[argv.index("--lay-out") + 1]
    return dry_run, lay_out


def role_prefix(obj):
    """Return the prefix this object should carry, or None to leave it alone.

    Precedence matches Nier_Fbx_Rule::Classify: Col -> Instance -> Shadow -> Lod -> Mesh.
    """
    names = {c.name for c in obj.users_collection}

    for collection_name, prefix in COLLECTION_PREFIX:
        if collection_name in names:
            return prefix

    group = obj.get("mesh_group_props")
    if group is not None:
        lod_name = group.get("lod_name")
        lod_level = group.get("lod_level")

        # lod_level -1 / lod_name SHADOW is a shadow-only pass, never an LOD tier.
        if lod_name == "SHADOW" or lod_level == -1:
            return "SHADOW"
        if isinstance(lod_level, int) and lod_level >= 1:
            return "LOD%d" % lod_level

    # LOD0 and anything unlabelled is ordinary visual geometry: no prefix.
    return None


def already_stamped(name, prefix):
    return name.upper().startswith(prefix.upper() + "_")


def stamp(dry_run):
    counts = {}
    renamed = 0
    skipped_tree = 0

    for obj in bpy.data.objects:
        names = {c.name for c in obj.users_collection}
        if names & set(TREE_COLLECTIONS):
            skipped_tree += 1
            continue

        prefix = role_prefix(obj)
        key = prefix or "Mesh"
        counts[key] = counts.get(key, 0) + 1

        if prefix is None or already_stamped(obj.name, prefix):
            continue

        new_name = "%s_%s" % (prefix, obj.name)
        print("  %-46s -> %s" % (obj.name, new_name))
        if not dry_run:
            obj.name = new_name
            # The exporter names FBX meshes after the mesh datablock, so stamp it too.
            if obj.data is not None and not already_stamped(obj.data.name, prefix):
                obj.data.name = "%s_%s" % (prefix, obj.data.name)
        renamed += 1

    return counts, renamed, skipped_tree


def dump_lay(path):
    """LAY placements are Empties and never survive an FBX mesh export."""
    records = []
    for obj in bpy.data.objects:
        names = {c.name for c in obj.users_collection}
        if not (names & set(LAY_COLLECTIONS)):
            continue
        records.append({
            "name": obj.name,
            "collections": sorted(names),
            "location": list(obj.location),
            "rotation_euler": list(obj.rotation_euler),
            "scale": list(obj.scale),
            "matrix_world": [list(row) for row in obj.matrix_world],
            "id_properties": {k: obj[k] for k in obj.keys() if k != "_RNA_UI"},
        })

    with open(path, "w", encoding="utf-8") as handle:
        json.dump({"placements": records}, handle, ensure_ascii=False, indent=1, default=str)
    return len(records)


def main():
    dry_run, lay_out = parse_args()
    blend_path = bpy.data.filepath
    print("=" * 70)
    print("stamp_nier_roles  %s%s" % (blend_path, "  [DRY RUN]" if dry_run else ""))
    print("=" * 70)

    counts, renamed, skipped_tree = stamp(dry_run)

    print("-" * 70)
    for key in sorted(counts):
        print("  %-10s %5d" % (key, counts[key]))
    print("  renamed    %5d" % renamed)
    print("  tree nodes skipped %d (col_colTreeNodes / wmb_colTreeNodes)" % skipped_tree)

    if lay_out is None and blend_path:
        lay_out = os.path.splitext(blend_path)[0] + "_lay.json"
    if lay_out and not dry_run:
        print("  LAY placements -> %s (%d)" % (lay_out, dump_lay(lay_out)))

    if not dry_run and blend_path:
        bpy.ops.wm.save_mainfile()
        print("  saved %s" % blend_path)


if __name__ == "__main__":
    main()
