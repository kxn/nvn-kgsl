#!/usr/bin/env python3
"""Build the standalone NativeActivity probe and record its source/artifact hashes."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import zipfile
from native_contract import native_contract


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_map(root, out):
    return {str(p.relative_to(root)): sha(p) for p in sorted(root.rglob('*'))
            if p.is_file() and p != out and out not in p.parents and '__pycache__' not in p.parts}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compute-driverparams-check',type=Path,help='Verified own NumWorkgroups raw-native controlled CS; no NVN/original shader claim')
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--ndk', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--cs-check', type=Path,
                        help='Successful check_ir3.py record; embed its first verified CS artifact')
    parser.add_argument('--keystore', type=Path, help='Reuse an existing local probe signing identity')
    parser.add_argument('--cs-case', type=int, default=0, help='Case index in the CS check record')
    parser.add_argument('--nvn-api', action='store_true', help='Run the native NVN resource/clear test')
    parser.add_argument('--compute-call',action='store_true',help='Qualify inherited direct/indirect public Call; requires --compute-indirect --maxwell-check')
    parser.add_argument('--compute-indirect',action='store_true',help='Qualify four original CS using GPU-produced indirect dimensions; no NumWorkgroups consumer claim')
    parser.add_argument('--maxwell-check', type=Path, help='Verified real D2R Maxwell compiler check record')
    parser.add_argument('--graphics-check', type=Path, help='Verified original VS/FS and native graphics state record')
    parser.add_argument('--graphics-draw-batch', action='store_true', help='Qualify four NVN indexed/instanced/indirect draw modes with real GPU count writes and handle replay')
    parser.add_argument('--nvn-graphics-check', type=Path, help='Verified original VS/FS, driven through NVN graphics calls')
    parser.add_argument('--nvn-external-image-check', type=Path, help='Verified schema3 original VS/FS; NVN draws into explicitly imported AHB targets')
    parser.add_argument('--fullsize-display-check','--kernel-check',dest='fullsize_display_check',type=Path,help='Verified authored linear-copy kernel check; raw2880x1620 AHB compute/display, not NVN Draw')
    parser.add_argument('--fullsize-display-slots',type=int,choices=(2,3),help='Raw fullsize AHB2/3 slots, default2')
    parser.add_argument('--bc-sampled-check',type=Path,help='Normal BC1/2/3 private sampled shadow raw-native qualifier; ordinary app UID')
    parser.add_argument('--texture-texels-check',type=Path,help='Independent public Write/Read/Flush/Invalidate GPU qualifier; checked original block kernel')
    parser.add_argument('--volume-texture-check',type=Path,help='Independent authored native3D TXF/TXS GPU qualifier; checked native shaders+owner kernel+FDL inputs')
    parser.add_argument('--buffer-texture-check',type=Path,help='Independent authored raw-native BUFFER TXF/TXS GPU qualification')
    parser.add_argument('--volume-shadow-check',type=Path,help='Independent true3D shadow-owner GPU materialization: checked kernel+runtime FDL inputs')
    parser.add_argument('--public-volume-copy-check',type=Path,help='Independent real public true3D volume Copy qualifier; checked normal volume kernel')
    parser.add_argument('--public-packaged-copy-check',type=Path,help='Explicit experimental BNTX writer8B profile: normal raw-block kernel, independent72case public Copy')
    parser.add_argument('--clear-color-check',type=Path,help='Independent public ClearColor/Call22format normal MetaClear qualification')
    parser.add_argument('--public-copy-check',type=Path,help='Normal block kernel; authored public NVN B2T/T2B/T2T CopyCommands234job qualifier')
    parser.add_argument('--public-copy-call',action='store_true',help='Use actual public CallCommands with ordinary array/volume qualifier; defaultOFF')
    parser.add_argument('--block-copy-check',type=Path,help='Normal general raw-block kernel check; authored148job/296submit qualification only')
    parser.add_argument('--texture-conversion-check',type=Path,help='Verified authored layout-copy kernels/source-payload/actualFDL batch; no original game/NVN texture claim')
    parser.add_argument('--nvn-texture-bridge-check',type=Path,help='Verified original sampled pair plus production GOB consumer bridge qualification')
    parser.add_argument('--nvn-sampler-check',type=Path,help='Verified schema3 A650 original2x2 pair; dynamic filter/wrap/minmag/singlemip batch')
    parser.add_argument('--nvn-counter-check', type=Path, help='Verified A650 schema3 color pair; actual GPU counters and context isolation')
    parser.add_argument('--nvn-copy-check', type=Path, help='Verified A650 schema3 color pair; ordinary CPU_NO_ACCESS NVN Draw to independent AHB GPU copy')
    parser.add_argument('--nvn-window-check', type=Path, help='Verified schema3 VS/FS; standard NVN Window lifecycle qualifier')
    parser.add_argument('--window-slots', type=int, choices=(2, 3), help='Number of external NVN Window slots (default2)')
    parser.add_argument('--external-image-slots', type=int, choices=(2, 3), help='Number of separate external targets (default2 with external qualification)')
    parser.add_argument('--texture-check', type=Path, help='Verified original sampled VS/FS and native texture state record')
    parser.add_argument('--nvn-texture-check', type=Path, help='Verified original sampled pair, driven through NVN descriptor pools and bindings')
    parser.add_argument('--texture-device-flags', type=lambda v: int(v, 0), choices=(0, 0x100),
                        help='Explicit device flags for the NVN texture qualifier; omit to test SetDefaults only')
    parser.add_argument('--texture-noaccess-pool', action='store_true',
                        help='Qualify original shader sampling from a CPU_NO_ACCESS|GPU_CACHED|COMPRESSIBLE pool')
    parser.add_argument('--texture-modes', action='store_true',
                        help='Qualify explicit NVN origin/depth modes with mirrored and clipped original shader cases')
    parser.add_argument('--texture-descriptor-flags', type=lambda v: int(v, 0), choices=(0x12, 0x14),
                        help='Qualify CPU-only software descriptor tables; actual GPU data remains cached')
    parser.add_argument('--texture-uncached-pools', action='store_true',
                        help='Qualify same-page CPU_UNCACHED pools using hardware IO coherency without public cache calls')
    parser.add_argument('--device-inspect', action='store_true', help='Only query KGSL properties; submit no GPU commands')
    parser.add_argument('--iocoherent-inspect', action='store_true', help='Qualify WB+IOCOHERENT ADDR import flags only; no GPU commands or NVN uncached claim')
    parser.add_argument('--ahb-inspect', action='store_true', help='Qualify AHardwareBuffer allocation, fd metadata and CPU mapping; no GPU submission')
    parser.add_argument('--ahb-graphics-check', type=Path, help='Verified original VS/FS; directly render into imported Android HardwareBuffer')
    parser.add_argument('--ahb-present-check', type=Path, help='Present original GPU-rendered AHB and retain Activity for window lifecycle tests')
    parser.add_argument('--d2r-host-record', type=Path, help='Verified original CLI host record; bundle its executable unchanged for separate run-as stages')
    parser.add_argument('--window-source-slots',type=int,choices=(2,3),help='Standard NVN GOB source Window/AHB/SC N2/3 qualifier')
    parser.add_argument('--texture-bridge-check',type=Path,help='Verified normal native-texture-consumer-bridge-v1 bundle for Window source qualification')
    parser.add_argument('--texture-bridge-diagnostics',action='store_true',help='Consumer qualification only: CP stage marks in retained BOs; not a fault PC, timer or completion proof')
    parser.add_argument('--native-pipeline-check',type=Path,help='Verified original color shader recipe and actual FDL dynamic raw-native72submit qualification; no NVN/game claim')
    parser.add_argument('--casc-gather-depth-check',type=Path,help='Independent unchanged original CASC public Gather+FragDepth72; default OFF')
    parser.add_argument('--nvn-color-target-check',type=Path,help='Independent original color public GOB22 overwrite/blend/mask132 qualifier, default OFF')
    parser.add_argument('--native-front-facing-check',type=Path,help='Independent authored halfFACE/FragCoord/FragDepth384 qualification; no public NVN/game')
    parser.add_argument('--native-depth-check',type=Path,help='Authored D16/D24X8/D32F GPU depth writes/tests/CP_BLIT readback, no public NVN/game claim')
    parser.add_argument('--native-depth-sampled-check',type=Path,help='Independent raw native noncompare depth texture60 qualification')
    parser.add_argument('--native-depth-shadow-check',type=Path,help='Independent raw canonical-depth GOB/CS/A2DUPLOAD/Draw/READBACK/GOB648 qualifier')
    parser.add_argument('--native-gather-check',type=Path,help='Authored raw-native TG4 components/sparse binding/wrap/replay, never public NVN/game')
    parser.add_argument('--native-sampler-mip-check',type=Path,help='Authored normalized2D mip/minmag/wrap/LOD sampler qualifier, not NVN/game')
    parser.add_argument('--native-color-target-check',type=Path,help='Normal actualFDL22 COLOR original bounded color shader132 raw-native qualification')
    parser.add_argument('--nvn-pipeline-check',type=Path,help='Checked public NVN original color source/dynamic Draw LINEAR64 qualifier')
    parser.add_argument('--nvn-pipeline-call',action='store_true',help='Public CallCommands wrapper with parent UBO0 and retired source; only with --nvn-pipeline-check')
    parser.add_argument('--nvn-pipeline-sampled-check',type=Path,help='Independent public original-source FLOAT2D22 full/selected mip264 qualifier; default OFF')
    parser.add_argument('--nvn-pipeline-bc-mips-check',type=Path,help='Independent public NVN BC UNORM/sRGB four-mip selected-view96 qualifier; normal default OFF')
    parser.add_argument('--nvn-pipeline-buffer-check',type=Path,help='Independent public NVN original C1 two-resource FloatTXF12 index0 qualification; default OFF')
    parser.add_argument('--nvn-pipeline-bc-check',type=Path,help='Checked PUBLIC NVN BC1/2/3 sampled source/descriptor/late-registration qualifier; bounded original a767 profile')
    parser.add_argument('--nvn-pipeline-gob-check',type=Path,help='Separate checked public NVN canonical GOB flags9 dynamic color Draw qualifier')
    parser.add_argument('--nvn-pipeline-runtime-views-check',type=Path,help='Independent public NVN noncatalogued target dimensions with actual runtime FDL metadata')
    args = parser.parse_args()
    if args.native_front_facing_check and (args.cs_case or any(v for k,v in vars(args).items()if k not in ('sdk','ndk','out','keystore','native_front_facing_check','cs_case')and v)):
        parser.error('--native-front-facing-check is an independent authored native qualification')
    if args.native_depth_sampled_check and (args.cs_case or any(v for k,v in vars(args).items()if k not in ('sdk','ndk','out','keystore','native_depth_sampled_check','cs_case')and v)):
        parser.error('--native-depth-sampled-check is an independent sampled-depth qualification')
    if args.casc_gather_depth_check and (args.cs_case or any(v for k,v in vars(args).items()if k not in ('sdk','ndk','out','keystore','casc_gather_depth_check','cs_case')and v)):
        parser.error('--casc-gather-depth-check is an independent public CASC qualification')
    if args.native_depth_shadow_check and (args.cs_case or any(v for k,v in vars(args).items()if k not in ('sdk','ndk','out','keystore','native_depth_shadow_check','cs_case')and v)):
        parser.error('--native-depth-shadow-check is an independent canonical-depth qualification')
    if args.nvn_pipeline_sampled_check and (args.cs_case or any(v for k,v in vars(args).items()if k not in ('sdk','ndk','out','keystore','nvn_pipeline_sampled_check','cs_case')and v)):
        parser.error('--nvn-pipeline-sampled-check is an independent public sampled22 qualification')
    if args.clear_color_check and (args.cs_case or any(v for k,v in vars(args).items()if k not in ('sdk','ndk','out','keystore','clear_color_check','cs_case')and v)):
        parser.error('--clear-color-check is an independent public NVN qualification')
    if args.nvn_color_target_check and (args.cs_case or any(v for k,v in vars(args).items()if k not in ('sdk','ndk','out','keystore','nvn_color_target_check','cs_case')and v)):
        parser.error('--nvn-color-target-check is an independent public GOB22 qualification')
    if args.nvn_pipeline_buffer_check and (args.cs_case or any(v for k,v in vars(args).items()if k not in ('sdk','ndk','out','keystore','nvn_pipeline_buffer_check','cs_case')and v)):
        parser.error('--nvn-pipeline-buffer-check is an independent public NVN BUFFER qualification')
    if args.buffer_texture_check and (args.cs_case or any(v for k,v in vars(args).items()if k not in ('sdk','ndk','out','keystore','buffer_texture_check','cs_case')and v)):
        parser.error('--buffer-texture-check is an independent raw-native qualification')
    if args.volume_texture_check and (args.cs_case or any(v for k,v in vars(args).items()if k not in ('sdk','ndk','out','keystore','volume_texture_check','cs_case')and v)):
        parser.error('--volume-texture-check is an independent raw-native qualification')
    if args.public_packaged_copy_check and (args.cs_case or any(v for k,v in vars(args).items()if k not in ('sdk','ndk','out','keystore','public_packaged_copy_check','cs_case')and v)):
        parser.error('--public-packaged-copy-check is an independent defaultOFF experiment')
    if args.nvn_pipeline_runtime_views_check and (args.cs_case or any(v for k,v in vars(args).items() if k not in ('sdk','ndk','out','keystore','nvn_pipeline_runtime_views_check','cs_case') and v)):
        parser.error('--nvn-pipeline-runtime-views-check is an independent public NVN qualification')
    if args.volume_shadow_check and (args.cs_case or any(v for k,v in vars(args).items()if k not in ('sdk','ndk','out','keystore','volume_shadow_check','cs_case')and v)):
        parser.error('--volume-shadow-check is an independent raw-native qualification')
    if args.nvn_pipeline_bc_mips_check and (args.cs_case or any(v for k,v in vars(args).items() if k not in ('sdk','ndk','out','keystore','nvn_pipeline_bc_mips_check','cs_case') and v)):
        parser.error('--nvn-pipeline-bc-mips-check is an independent public NVN BC mip qualification')
    if args.nvn_pipeline_bc_check and (args.cs_case or any(v for k,v in vars(args).items() if k not in ('sdk','ndk','out','keystore','nvn_pipeline_bc_check','cs_case') and v)):
        parser.error('--nvn-pipeline-bc-check is an independent public NVN BC qualification')
    if args.public_copy_call and not (args.public_copy_check or args.public_volume_copy_check):
        parser.error('--public-copy-call requires an ordinary --public-copy-check or --public-volume-copy-check')
    if args.public_volume_copy_check and any(v for k,v in vars(args).items()if k not in ('sdk','ndk','out','keystore','public_volume_copy_check','public_copy_call','cs_case')and v):
        parser.error('--public-volume-copy-check is an independent true3D qualification operation')
    if args.public_volume_copy_check and args.cs_case:
        parser.error('--public-volume-copy-check has no CS case submode')
    if args.bc_sampled_check and any(v for k,v in vars(args).items() if k not in ('sdk','ndk','out','keystore','bc_sampled_check','cs_case') and v):
        parser.error('--bc-sampled-check is an independent raw-native BC qualifier')
    if args.bc_sampled_check and args.cs_case:
        parser.error('--bc-sampled-check has no CS-case submode')
    if args.texture_texels_check and any(v for k,v in vars(args).items() if k not in ('sdk','ndk','out','keystore','texture_texels_check','cs_case') and v):
        parser.error('--texture-texels-check is an independent operation; other modes/submodes are incompatible')
    if args.texture_texels_check and args.cs_case:
        parser.error('--texture-texels-check is an independent operation; no CS case selection')
    if args.public_copy_check and any(v for k,v in vars(args).items() if k not in ('sdk','ndk','out','keystore','public_copy_check','public_copy_call','cs_case') and v):
        parser.error('--public-copy-check is an independent public NVN copy qualifier')
    if args.public_copy_check and args.cs_case:
        parser.error('--public-copy-check has no CS-case submode')
    if args.block_copy_check and any(v for k,v in vars(args).items() if k not in ('sdk','ndk','out','keystore','block_copy_check','cs_case') and v):
        parser.error('--block-copy-check is an independent authored raw-block qualifier')
    if args.block_copy_check and args.cs_case:
        parser.error('--block-copy-check has no CS-case submode')
    if args.nvn_pipeline_gob_check and any(v for k,v in vars(args).items() if k not in ('sdk','ndk','out','keystore','nvn_pipeline_gob_check','cs_case') and v):
        parser.error('--nvn-pipeline-gob-check is a separate public NVN qualifier')
    if args.nvn_pipeline_call and not args.nvn_pipeline_check:parser.error('--nvn-pipeline-call requires --nvn-pipeline-check')
    if args.nvn_pipeline_check and any(v for k,v in vars(args).items() if k not in ('sdk','ndk','out','keystore','nvn_pipeline_check','nvn_pipeline_call','cs_case') and v):
        parser.error('--nvn-pipeline-check is an independent public NVN qualifier')
    if args.native_depth_check and any(v for k,v in vars(args).items()if k not in ('sdk','ndk','out','keystore','native_depth_check','cs_case')and v):
        parser.error('--native-depth-check is an independent raw-native qualifier')
    if args.native_sampler_mip_check and any(v for k,v in vars(args).items()if k not in ('sdk','ndk','out','keystore','native_sampler_mip_check','cs_case')and v):
        parser.error('--native-sampler-mip-check is an independent authored raw-native qualifier')
    if args.native_gather_check and any(v for k,v in vars(args).items()if k not in ('sdk','ndk','out','keystore','native_gather_check','cs_case')and v):
        parser.error('--native-gather-check is an independent authored raw-native qualifier')
    if args.native_color_target_check and any(v for k,v in vars(args).items() if k not in ('sdk','ndk','out','keystore','native_color_target_check','cs_case') and v):
        parser.error('--native-color-target-check is an independent raw-native qualifier')
    if args.native_pipeline_check and any(v for k,v in vars(args).items() if k not in ('sdk','ndk','out','keystore','native_pipeline_check','cs_case') and v):
        parser.error('--native-pipeline-check is an independent raw-native qualifier')
    if args.texture_bridge_diagnostics and not args.nvn_texture_bridge_check:
        parser.error('--texture-bridge-diagnostics requires --nvn-texture-bridge-check')
    if args.nvn_texture_bridge_check and any((args.nvn_api,args.cs_check,args.maxwell_check,args.graphics_check,
            args.nvn_graphics_check,args.nvn_external_image_check,args.nvn_window_check,args.nvn_copy_check,args.nvn_counter_check,args.nvn_sampler_check,
            args.texture_conversion_check,args.texture_check,args.nvn_texture_check,args.device_inspect,args.iocoherent_inspect,args.ahb_inspect,
            args.ahb_graphics_check,args.ahb_present_check,args.d2r_host_record,args.graphics_draw_batch,args.compute_driverparams_check,args.compute_indirect,
            args.fullsize_display_check,args.fullsize_display_slots is not None,args.texture_device_flags is not None,args.texture_noaccess_pool,
            args.texture_modes,args.texture_uncached_pools,args.texture_descriptor_flags is not None,args.window_slots is not None,args.external_image_slots is not None)):
        parser.error('--nvn-texture-bridge-check is an independent NVN qualifier')
    if (args.window_source_slots is None) != (args.texture_bridge_check is None):
        parser.error('--window-source-slots and --texture-bridge-check require each other')
    if args.window_source_slots and any(v for k,v in vars(args).items() if k not in ('sdk','ndk','out','keystore','window_source_slots','texture_bridge_check','cs_case') and v):
        parser.error('Window source qualification is an independent mode')
    if args.fullsize_display_slots is not None and not args.fullsize_display_check:
        parser.error('--fullsize-display-slots requires --fullsize-display-check')
    if args.fullsize_display_check and any((args.nvn_api,args.cs_check,args.maxwell_check,args.graphics_check,
            args.nvn_graphics_check,args.nvn_external_image_check,args.nvn_window_check,args.nvn_copy_check,args.nvn_counter_check,args.nvn_sampler_check,args.nvn_texture_bridge_check,
            args.texture_conversion_check,args.texture_check,args.nvn_texture_check,args.device_inspect,args.iocoherent_inspect,args.ahb_inspect,
            args.ahb_graphics_check,args.ahb_present_check,args.d2r_host_record,args.graphics_draw_batch,args.compute_driverparams_check,args.compute_indirect,
            args.texture_device_flags is not None,args.texture_noaccess_pool,args.texture_modes,args.texture_uncached_pools,
            args.texture_descriptor_flags is not None,args.window_slots is not None,args.external_image_slots is not None)):
        parser.error('--fullsize-display-check is an independent raw authored compute/display qualifier')
    fullsize_slots=(args.fullsize_display_slots or 2) if args.fullsize_display_check else None
    if args.texture_conversion_check and any((args.nvn_api,args.cs_check,args.maxwell_check,args.graphics_check,
            args.nvn_graphics_check,args.nvn_external_image_check,args.nvn_window_check,args.nvn_copy_check,args.nvn_counter_check,args.nvn_sampler_check,args.nvn_texture_bridge_check,
            args.texture_check,args.nvn_texture_check,args.device_inspect,args.iocoherent_inspect,args.ahb_inspect,
            args.ahb_graphics_check,args.ahb_present_check,args.d2r_host_record,args.graphics_draw_batch,args.compute_driverparams_check,args.compute_indirect,
            args.texture_device_flags is not None,args.texture_noaccess_pool,args.texture_modes,args.texture_uncached_pools,
            args.texture_descriptor_flags is not None,args.window_slots is not None,args.external_image_slots is not None)):
        parser.error('--texture-conversion-check is an independent authored shader qualifier')
    if args.nvn_sampler_check and any((args.nvn_api,args.cs_check,args.maxwell_check,args.graphics_check,
            args.nvn_graphics_check,args.nvn_external_image_check,args.nvn_window_check,args.nvn_copy_check,args.nvn_counter_check,
            args.texture_check,args.nvn_texture_check,args.device_inspect,args.iocoherent_inspect,args.ahb_inspect,
            args.ahb_graphics_check,args.ahb_present_check,args.d2r_host_record,args.graphics_draw_batch,args.compute_driverparams_check,args.compute_indirect,
            args.texture_device_flags is not None,args.texture_noaccess_pool,args.texture_modes,args.texture_uncached_pools,
            args.texture_descriptor_flags is not None,args.window_slots is not None,args.external_image_slots is not None)):
        parser.error('--nvn-sampler-check is an independent qualifier with no submodes')
    if args.compute_driverparams_check and any((args.nvn_api,args.cs_check,args.maxwell_check,args.graphics_check,args.nvn_graphics_check,args.nvn_external_image_check,args.nvn_window_check,args.texture_check,args.nvn_texture_check,args.device_inspect,args.iocoherent_inspect,args.ahb_inspect,args.ahb_graphics_check,args.ahb_present_check,args.nvn_copy_check,args.d2r_host_record)):
        parser.error('--compute-driverparams-check is an independent own raw-native qualifier')
    if args.nvn_counter_check and any((args.nvn_api,args.cs_check,args.maxwell_check,args.graphics_check,
            args.nvn_graphics_check,args.nvn_external_image_check,args.nvn_window_check,args.nvn_copy_check,
            args.texture_check,args.nvn_texture_check,args.device_inspect,args.iocoherent_inspect,args.ahb_inspect,
            args.ahb_graphics_check,args.ahb_present_check,args.d2r_host_record,args.graphics_draw_batch,args.compute_driverparams_check,args.compute_indirect)):
        parser.error('--nvn-counter-check is an independent qualifier')

    if args.compute_call and (not args.compute_indirect or not args.maxwell_check):
        parser.error('--compute-call requires --compute-indirect --maxwell-check')
    if args.compute_indirect and not args.maxwell_check:
        parser.error('--compute-indirect requires --maxwell-check and no other operation')
    if args.graphics_draw_batch and not args.nvn_graphics_check:
        parser.error('--graphics-draw-batch requires --nvn-graphics-check and no other operation')
    if args.nvn_copy_check and any((args.nvn_api, args.cs_check, args.maxwell_check, args.graphics_check,
            args.nvn_graphics_check, args.nvn_external_image_check, args.nvn_window_check,
            args.texture_check, args.nvn_texture_check, args.device_inspect, args.iocoherent_inspect,
            args.ahb_inspect, args.ahb_graphics_check, args.ahb_present_check, args.d2r_host_record)):
        parser.error('--nvn-copy-check is an independent qualifier; select no other operation/host')
    if args.window_slots is not None and not args.nvn_window_check:
        parser.error('--window-slots requires --nvn-window-check')
    window_slots = (args.window_slots or 2) if args.nvn_window_check else None
    if args.external_image_slots is not None and not args.nvn_external_image_check:
        parser.error('--external-image-slots requires --nvn-external-image-check')
    external_slots = (args.external_image_slots or 2) if args.nvn_external_image_check else None
    if args.texture_device_flags is not None and not args.nvn_texture_check:
        parser.error('--texture-device-flags requires --nvn-texture-check')
    if args.texture_noaccess_pool and not args.nvn_texture_check:
        parser.error('--texture-noaccess-pool requires --nvn-texture-check')
    if args.texture_modes and not args.nvn_texture_check:
        parser.error('--texture-modes requires --nvn-texture-check')
    if args.texture_uncached_pools and not args.nvn_texture_check:
        parser.error('--texture-uncached-pools requires --nvn-texture-check')
    if args.texture_descriptor_flags is not None:
        if not args.nvn_texture_check:
            parser.error('--texture-descriptor-flags requires --nvn-texture-check')
        incompatible = (args.texture_uncached_pools, args.cs_check, args.maxwell_check,
                        args.graphics_check, args.nvn_graphics_check, args.nvn_external_image_check,
                        args.nvn_window_check, args.texture_check, args.device_inspect,
                        args.iocoherent_inspect, args.ahb_inspect, args.ahb_graphics_check,
                        args.ahb_present_check, args.nvn_api, args.d2r_host_record, args.nvn_copy_check)
        if any(incompatible):
            parser.error('descriptor-only qualifier requires cached GPU data and only the NVN texture operation')
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve()
    # Output must not be the source tree or its ancestor.
    if out == root or out in root.parents:
        parser.error('output must be a dedicated build directory')
    out.mkdir(parents=True, exist_ok=True)
    previous_path = out / 'build-record.json'
    if previous_path.exists():
        previous = json.loads(previous_path.read_text())
        # Validate cached products before CMake/bundling can overwrite or
        # reuse them. A normal rebuild must not bless a modified old binary.
        for field in ('artifacts', 'compiled_inputs'):
            for name, expected in previous.get(field, {}).items():
                path = Path(name)
                if not path.is_file() or sha(path) != expected:
                    parser.error('cached ' + field + ' integrity mismatch: ' + name + '; use a fresh output')
    sdk, ndk = args.sdk.resolve(), args.ndk.resolve()
    bt = sdk / 'build-tools/36.0.0'
    commands = []
    source = source_map(root, out)
    tool_paths = [bt / name for name in ('aapt2', 'zipalign', 'apksigner')]
    tool_paths += [bt / 'lib/apksigner.jar', sdk / 'platforms/android-36/android.jar']
    tool_paths += [Path(shutil.which(name)) for name in ('cmake', 'ninja', 'keytool')]
    tool_paths += [ndk / 'source.properties', ndk / 'build/cmake/android.toolchain.cmake',
                   ndk / 'toolchains/llvm/prebuilt/linux-x86_64/bin/clang']
    if args.ahb_present_check or args.nvn_window_check or args.fullsize_display_check or args.window_source_slots:
        tool_paths += [bt / 'd8', Path(shutil.which('javac'))]
    tools = {str(p): sha(p) for p in tool_paths}

    def run(*command):
        argv = list(map(str, command))
        commands.append(argv)
        subprocess.run(argv, check=True)

    d2r_host = None
    if args.d2r_host_record:
        d2r_host = json.loads(args.d2r_host_record.read_text())
        for field in ('artifacts', 'sources', 'tools'):
            for name, expected in d2r_host[field].items():
                if sha(Path(name)) != expected:
                    parser.error('D2R host provenance mismatch: ' + name)
        host_options = d2r_host['options']
        verify_host = [host_options['python'], root / 'tools/build_d2r_host.py',
            '--exefs', host_options['exefs'], '--ndk', host_options['ndk'],
            '--out', args.d2r_host_record.resolve().parent, '--python', host_options['python'],
            '--jobs', host_options['jobs'], '--verify']
        for option in ('compute_cache_check','texture_bridge_check','graphics_pipeline_check','block_copy_check','bc_sampled_fdl_check'):
            if host_options.get(option):verify_host += ['--'+option.replace('_','-'), host_options[option]]
        run(*verify_host)

    compute_indirect_manifest = None
    driverparams_check, driverparams_header = None, ''
    shader = None
    shader_options = ['-DNVN_CS_HEADER=', '-DNVN_MAXWELL_HEADER=']
    if sum(bool(v) for v in (args.nvn_api, args.cs_check, args.maxwell_check, args.graphics_check, args.nvn_graphics_check, args.nvn_external_image_check, args.nvn_window_check, args.texture_check, args.nvn_texture_check, args.device_inspect, args.iocoherent_inspect, args.ahb_inspect, args.ahb_graphics_check, args.ahb_present_check, args.nvn_copy_check, args.nvn_counter_check, args.nvn_sampler_check,args.nvn_texture_bridge_check, args.texture_conversion_check)) > 1:
        parser.error('select one qualification mode')
    maxwell, bundle_hashes = None, {}
    nvn_pipeline_check,nvn_pipeline_header,nvn_pipeline_bc_library=None,'',''
    nvn_pipeline_runtime_library=''
    casc_depth_libraries=''
    nvn_pipeline_buffer_library=''
    if args.casc_gather_depth_check or args.nvn_pipeline_sampled_check or args.nvn_color_target_check or args.nvn_pipeline_buffer_check or args.nvn_pipeline_runtime_views_check or args.nvn_pipeline_check or args.nvn_pipeline_gob_check or args.nvn_pipeline_bc_check or args.nvn_pipeline_bc_mips_check:
        spec=importlib.util.spec_from_file_location('nvn_pipeline_qualification',root/('tools/casc_gather_depth_qualification.py' if args.casc_gather_depth_check else 'tools/nvn_pipeline_sampled_qualification.py' if args.nvn_pipeline_sampled_check else 'tools/nvn_pipeline_color_qualification.py' if args.nvn_color_target_check else 'tools/nvn_pipeline_buffer_qualification.py' if args.nvn_pipeline_buffer_check else 'tools/nvn_pipeline_runtime_views_qualification.py' if args.nvn_pipeline_runtime_views_check else 'tools/nvn_pipeline_bc_mip_qualification.py' if args.nvn_pipeline_bc_mips_check else 'tools/nvn_pipeline_bc_qualification.py' if args.nvn_pipeline_bc_check else 'tools/nvn_pipeline_gob_qualification.py' if args.nvn_pipeline_gob_check else 'tools/nvn_pipeline_qualification.py'))
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        header,nvn_pipeline_check,bundle_hashes=module.build((args.casc_gather_depth_check or args.nvn_pipeline_sampled_check or args.nvn_color_target_check or args.nvn_pipeline_buffer_check or args.nvn_pipeline_runtime_views_check or args.nvn_pipeline_bc_mips_check or args.nvn_pipeline_bc_check or args.nvn_pipeline_gob_check or args.nvn_pipeline_check).resolve(),out/'compiled',root=root.parent)
        nvn_pipeline_header=str(header.resolve())
        if args.casc_gather_depth_check:casc_depth_libraries=';'.join(nvn_pipeline_check['native_libraries'])
        if args.nvn_pipeline_bc_check or args.nvn_pipeline_bc_mips_check:nvn_pipeline_bc_library=nvn_pipeline_check['runtime_fdl_library']
        if args.casc_gather_depth_check or args.nvn_pipeline_sampled_check or args.nvn_color_target_check or args.nvn_pipeline_runtime_views_check:nvn_pipeline_runtime_library=nvn_pipeline_check['runtime_fdl_library']
        if args.nvn_pipeline_buffer_check:nvn_pipeline_buffer_library=nvn_pipeline_check['runtime_fdl_library']
    pipeline_check,pipeline_header,pipeline_color_library,pipeline_mip_library=None,'','',''
    pipeline_depth_libraries=''
    if args.native_front_facing_check or args.native_depth_sampled_check or args.native_depth_shadow_check or args.native_pipeline_check or args.native_color_target_check or args.native_gather_check or args.native_depth_check or args.native_sampler_mip_check:
        spec=importlib.util.spec_from_file_location('pipeline_qualification',root/('tools/front_facing_qualification.py'if args.native_front_facing_check else 'tools/depth_sampled_probe_qualification.py'if args.native_depth_sampled_check else 'tools/depth_shadow_probe_qualification.py'if args.native_depth_shadow_check else 'tools/sampler_mip_qualification.py'if args.native_sampler_mip_check else 'tools/depth_probe_qualification.py'if args.native_depth_check else 'tools/gather_probe_qualification.py'if args.native_gather_check else 'tools/color_target_qualification.py' if args.native_color_target_check else 'tools/pipeline_qualification.py'))
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        header,pipeline_check,bundle_hashes=module.build((args.native_front_facing_check or args.native_depth_sampled_check or args.native_depth_shadow_check or args.native_sampler_mip_check or args.native_depth_check or args.native_gather_check or args.native_color_target_check or args.native_pipeline_check).resolve(),out/'compiled',root=root.parent)
        pipeline_header=str(header.resolve())
        if args.native_front_facing_check or args.native_depth_sampled_check or args.native_depth_shadow_check or args.native_depth_check:pipeline_depth_libraries=';'.join(pipeline_check['native_libraries'])
        if args.native_color_target_check:pipeline_color_library=pipeline_check['runtime_fdl_library']
        if args.native_sampler_mip_check:pipeline_mip_library=pipeline_check['runtime_fdl_library']
    graphics, graphics_header = None, ''
    graphics_api, graphics_api_header = None, ''
    texture, texture_header = None, ''
    texture_api, texture_api_header = None, ''
    ahb_graphics, ahb_graphics_header = None, ''
    copy_check, copy_pm4_header = None, ''
    bc_sampled_check,bc_sampled_header,bc_sampled_library=None,'',''
    if args.bc_sampled_check:
        spec=importlib.util.spec_from_file_location('bc_sampled_bundle',root/'tools/bc_sampled_bundle.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        header,bc_sampled_check,bundle_hashes=module.build(args.bc_sampled_check.resolve(),out/'compiled',root=root.parent)
        bc_sampled_header=str(header.resolve());bc_sampled_library=bc_sampled_check['runtime_fdl_library']
    texture_texels_check,texture_texels_header=None,''
    if args.texture_texels_check:
        spec=importlib.util.spec_from_file_location('texture_texels_bundle',root/'tools/texture_texels_bundle.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        header,texture_texels_check,bundle_hashes=module.build(args.texture_texels_check.resolve(),out/'compiled',root=root.parent)
        texture_texels_header=str(header.resolve())
    buffer_texture_check,buffer_texture_header,buffer_texture_library=None,'',''
    if args.buffer_texture_check:
        spec=importlib.util.spec_from_file_location('buffer_texture_bundle',root/'tools/buffer_texture_bundle.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        header,buffer_texture_check,bundle_hashes=module.build(args.buffer_texture_check.resolve(),out/'compiled',root=root.parent)
        buffer_texture_header=str(header.resolve());buffer_texture_library=buffer_texture_check['runtime_fdl_library']
    volume_texture_check,volume_texture_header,volume_texture_library=None,'',''
    if args.volume_texture_check:
        spec=importlib.util.spec_from_file_location('volume_texture_bundle',root/'tools/volume_texture_bundle.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        header,volume_texture_check,bundle_hashes=module.build(args.volume_texture_check.resolve(),out/'compiled',root=root.parent)
        volume_texture_header=str(header.resolve());volume_texture_library=volume_texture_check['runtime_fdl_library']
    volume_shadow_check,volume_shadow_header,volume_shadow_library=None,'',''
    if args.volume_shadow_check:
        spec=importlib.util.spec_from_file_location('volume_shadow_bundle',root/'tools/volume_shadow_bundle.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        header,volume_shadow_check,bundle_hashes=module.build(args.volume_shadow_check.resolve(),out/'compiled',root=root.parent)
        volume_shadow_header=str(header.resolve());volume_shadow_library=volume_shadow_check['runtime_fdl_library']
    public_volume_copy_check,public_volume_copy_header=None,''
    if args.public_volume_copy_check:
        spec=importlib.util.spec_from_file_location('public_volume_copy_bundle',root/'tools/public_volume_copy_bundle.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        header,public_volume_copy_check,bundle_hashes=module.build(args.public_volume_copy_check.resolve(),out/'compiled',root=root.parent)
        public_volume_copy_header=str(header.resolve())
    public_packaged_copy_check,public_packaged_copy_header=None,''
    if args.public_packaged_copy_check:
        spec=importlib.util.spec_from_file_location('public_packaged_copy_bundle',root/'tools/public_packaged_copy_bundle.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        header,public_packaged_copy_check,bundle_hashes=module.build(args.public_packaged_copy_check.resolve(),out/'compiled',root=root.parent)
        public_packaged_copy_header=str(header.resolve())
    clear_color_check,clear_color_header,clear_color_library=None,'',''
    if args.clear_color_check:
        spec=importlib.util.spec_from_file_location('clear_color_qualification',root/'tools/clear_color_qualification.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        header,clear_color_check,bundle_hashes=module.build(args.clear_color_check.resolve(),out/'compiled',root=root.parent)
        clear_color_header=str(header.resolve());clear_color_library=clear_color_check['constant_library']
    public_copy_check,public_copy_header=None,''
    if args.public_copy_check:
        spec=importlib.util.spec_from_file_location('public_copy_bundle',root/'tools/public_copy_bundle.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        header,public_copy_check,bundle_hashes=module.build(args.public_copy_check.resolve(),out/'compiled',root=root.parent)
        public_copy_header=str(header.resolve())
    block_check,block_header=None,''
    if args.block_copy_check:
        spec=importlib.util.spec_from_file_location('block_copy_bundle',root/'tools/block_copy_bundle.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        header,block_check,bundle_hashes=module.build(args.block_copy_check.resolve(),out/'compiled',root=root.parent)
        block_header=str(header.resolve())
    conversion_check,conversion_header=None,''
    window_source_check,window_source_header=None,''
    if args.window_source_slots:
        spec=importlib.util.spec_from_file_location('window_source_bundle',root/'tools/texture_bridge_bundle.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        header,window_source_check=module.build(root.parent,args.texture_bridge_check.resolve())
        window_source_header=str(header.resolve())
        bundle_hashes.update(window_source_check['inputs']);bundle_hashes.update(window_source_check['artifacts'])
        bundle_hashes[str(args.texture_bridge_check.resolve())]=sha(args.texture_bridge_check.resolve())
        bundle_hashes[str(root/'tools/texture_bridge_bundle.py')]=sha(root/'tools/texture_bridge_bundle.py')
    fullsize_check,fullsize_header=None,str()
    if args.fullsize_display_check:
        spec=importlib.util.spec_from_file_location('fullsize_display',root/'tools/fullsize_display.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        header,fullsize_check,bundle_hashes=module.build(args.fullsize_display_check.resolve(),out/'compiled',root=root.parent)
        fullsize_header=str(header.resolve())
    bridge_check,bridge_header=None,''
    if args.nvn_texture_bridge_check:
        spec=importlib.util.spec_from_file_location('texture_bridge_qualification',root/'tools/texture_bridge_qualification.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        header,native_header,bridge_check,bundle_hashes=module.build(args.nvn_texture_bridge_check.resolve(),out/'compiled',root=root.parent)
        texture_api_header=str(header.resolve());bridge_header=str(native_header.resolve())
    if args.texture_conversion_check:
        spec=importlib.util.spec_from_file_location('texture_conversion',root/'tools/texture_conversion.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        header,conversion_check,bundle_hashes=module.build(args.texture_conversion_check.resolve(),out/'compiled',root=root.parent)
        conversion_header=str(header.resolve())
    if args.compute_driverparams_check:
        spec=importlib.util.spec_from_file_location('compute_driverparams',root/'tools/compute_driverparams.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        header,driverparams_check,bundle_hashes=module.build(args.compute_driverparams_check.resolve(),out/'compiled',root=root.parent)
        driverparams_header=str(header)
    if args.nvn_copy_check:
        import sys
        sys.path.insert(0, str(root / 'adapters/d2r'))
        spec = importlib.util.spec_from_file_location('copy_bundle', root / 'adapters/d2r/copy_bundle.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        header, copy_check, bundle_hashes, pm4 = module.build(args.nvn_copy_check.resolve(), out / 'compiled')
        graphics_api_header = str(header.resolve())
        copy_pm4_header = str((pm4 / 'adreno_pm4.xml.h').resolve())
    if args.ahb_graphics_check or args.ahb_present_check:
        spec = importlib.util.spec_from_file_location('ahb_graphics_bundle', root / 'adapters/d2r/graphics_bundle.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        header, ahb_graphics, bundle_hashes = module.build((args.ahb_graphics_check or args.ahb_present_check).resolve(), out / 'compiled')
        ahb_graphics_header = str(header.resolve())
    if args.nvn_texture_check or args.nvn_sampler_check:
        spec = importlib.util.spec_from_file_location('texture_api_bundle', root / 'adapters/d2r/texture_api_bundle.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        header, texture_api, bundle_hashes = module.build((args.nvn_texture_check or args.nvn_sampler_check).resolve(), out / 'compiled')
        texture_api_header = str(header.resolve())
        if args.nvn_sampler_check and (texture_api.get('modes') is not True or texture_api.get('image',{}).get('schema')!=3 or texture_api.get('program',{}).get('chip_id')!=0x06050002):
            parser.error('sampler qualifier requires checked schema3 A650 original sampled inputs')
    if args.texture_check:
        spec = importlib.util.spec_from_file_location('texture_bundle', root / 'adapters/d2r/texture_bundle.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        header, texture, bundle_hashes = module.build(args.texture_check.resolve(), out / 'compiled')
        texture_header = str(header.resolve())
    if args.nvn_graphics_check or args.nvn_external_image_check or args.nvn_window_check or args.nvn_counter_check:
        import sys
        sys.path.insert(0, str(root / 'adapters/d2r'))
        spec = importlib.util.spec_from_file_location('graphics_api_bundle', root / 'adapters/d2r/graphics_api_bundle.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        header, graphics_api, bundle_hashes = module.build((args.nvn_graphics_check or args.nvn_external_image_check or args.nvn_window_check or args.nvn_counter_check).resolve(), out / 'compiled')
        if args.graphics_draw_batch and (graphics_api.get('modes') is not True or graphics_api['image']['schema'] != 3 or graphics_api['program']['chip_id'] != 0x06050002):
            parser.error('draw batch requires an explicit A650 schema3 original shader image')
        if (args.nvn_external_image_check or args.nvn_window_check) and (graphics_api.get('modes') is not True or graphics_api['image']['schema'] != 3):
            parser.error('external NVN target qualification requires an explicit schema3 mode image')
        if args.nvn_counter_check and (graphics_api.get('modes') is not True or graphics_api['image']['schema']!=3 or graphics_api['program']['chip_id']!=0x06050002):
            parser.error('counter qualification requires original A650 schema3 color inputs')
        graphics_api_header = str(header.resolve())
    if args.graphics_check:
        spec = importlib.util.spec_from_file_location('graphics_bundle', root / 'adapters/d2r/graphics_bundle.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        header, graphics, bundle_hashes = module.build(args.graphics_check.resolve(), out / 'compiled')
        graphics_header = str(header.resolve())
    if args.maxwell_check:
        if args.compute_indirect:
            # Reuse the production validator's complete source/tool/compiler
            # closure; its generic cache is not linked or activated here.
            cache_spec=importlib.util.spec_from_file_location('compute_indirect_validation',root/'tools/offline_compute_cache.py')
            cache_module=importlib.util.module_from_spec(cache_spec);cache_spec.loader.exec_module(cache_module)
            unused_header,compute_indirect_manifest,cache_outputs=cache_module.build(args.maxwell_check.resolve(),out/'compute-verified',root=root.parent)
            if compute_indirect_manifest['chip_id']!=0x06050002 or len(compute_indirect_manifest['entries'])!=4:
                parser.error('compute indirect requires four full-control A650 originals')
        spec = importlib.util.spec_from_file_location('compute_bundle', root / 'adapters/d2r/compute_bundle.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        header, maxwell, bundle_hashes = module.build(args.maxwell_check.resolve(), out / 'compiled')
        if args.compute_indirect:
            bundle_hashes.update(compute_indirect_manifest['inputs']);bundle_hashes.update(cache_outputs)
        shader_options = ['-DNVN_CS_HEADER=', f'-DNVN_MAXWELL_HEADER={header.resolve()}']
    if args.cs_check:
        shader = json.loads(args.cs_check.read_text())
        if not shader['passed']:
            parser.error('CS compiler check did not pass')
        if args.cs_case < 0 or args.cs_case >= len(shader['cases']):
            parser.error('CS case index out of range')
        case = shader['cases'][args.cs_case]
        header = Path(case['command'][1]).with_suffix('.h')
        if sha(header) != case['artifacts'][header.name]:
            parser.error('CS artifact hash mismatch')
        shader_options = [f'-DNVN_CS_HEADER={header.resolve()}', '-DNVN_MAXWELL_HEADER=']
    run('cmake', '-S', root, '-B', out / 'cmake', '-G', 'Ninja', *shader_options,
        f'-DNVN_GRAPHICS_HEADER={graphics_header}',
        f'-DNVN_GRAPHICS_API_HEADER={graphics_api_header}',
        '-DNVN_GRAPHICS_DRAW_BATCH=' + ('ON' if args.graphics_draw_batch else 'OFF'),
        f'-DNVN_TEXTURE_HEADER={texture_header}',
        f'-DNVN_TEXTURE_API_HEADER={texture_api_header}',
        f'-DNVN_AHB_GRAPHICS_HEADER={ahb_graphics_header}',
        f'-DNVN_COMPUTE_DRIVERPARAMS_HEADER={driverparams_header}',
        '-DNVN_WINDOW_SOURCE_SLOTS=' + (str(args.window_source_slots) if args.window_source_slots else ''),
        '-DNVN_WINDOW_SOURCE_BRIDGE_HEADER=' + window_source_header,
        '-DNVN_FULLSIZE_DISPLAY_SLOTS=' + (str(fullsize_slots) if fullsize_slots else ''),
        '-DNVN_FULLSIZE_LINEAR_HEADER=' + fullsize_header,
        '-DNVN_WINDOW_SLOTS=' + (str(window_slots) if window_slots else ''),
        '-DNVN_EXTERNAL_IMAGE_SLOTS=' + (str(external_slots) if external_slots else ''),
        '-DNVN_TEXTURE_DEVICE_FLAGS=' + (str(args.texture_device_flags) if args.texture_device_flags is not None else ''),
        '-DNVN_TEXTURE_NOACCESS_POOL=' + ('1' if args.texture_noaccess_pool else '0'),
        '-DNVN_TEXTURE_MODES=' + ('1' if args.texture_modes else '0'),
        '-DNVN_TEXTURE_UNCACHED_POOLS=' + ('1' if args.texture_uncached_pools else '0'),
        '-DNVN_TEXTURE_DESCRIPTOR_FLAGS=' + str(args.texture_descriptor_flags or 0),
        '-DNVN_D2R_HOST_BRIDGE=OFF', '-DNVN_D2R_NAMES_HEADER=',
        '-DNVN_COMPUTE_CALL=' + ('ON' if args.compute_call else 'OFF'),
        '-DNVN_COMPUTE_INDIRECT=' + ('ON' if args.compute_indirect else 'OFF'),
        '-DNVN_BC_SAMPLED_PROBE=' + ('ON' if args.bc_sampled_check else 'OFF'),
        '-DNVN_BC_SAMPLED_HEADER=' + bc_sampled_header,
        '-DNVN_BC_SAMPLED_FDL_LIBRARY=' + bc_sampled_library,
        '-DNVN_TEXTURE_TEXELS_PROBE=' + ('ON' if args.texture_texels_check else 'OFF'),
        '-DNVN_TEXTURE_TEXELS_HEADER=' + texture_texels_header,
        '-DNVN_BUFFER_TEXTURE_PROBE=' + ('ON' if args.buffer_texture_check else 'OFF'),
        '-DNVN_BUFFER_TEXTURE_HEADER=' + buffer_texture_header,
        '-DNVN_BUFFER_TEXTURE_FDL_LIBRARY=' + buffer_texture_library,
        '-DNVN_VOLUME_TEXTURE_PROBE=' + ('ON' if args.volume_texture_check else 'OFF'),
        '-DNVN_VOLUME_TEXTURE_HEADER=' + volume_texture_header,
        '-DNVN_VOLUME_TEXTURE_FDL_LIBRARY=' + volume_texture_library,
        '-DNVN_VOLUME_SHADOW_PROBE=' + ('ON' if args.volume_shadow_check else 'OFF'),
        '-DNVN_VOLUME_SHADOW_HEADER=' + volume_shadow_header,
        '-DNVN_VOLUME_SHADOW_FDL_LIBRARY=' + volume_shadow_library,
        '-DNVN_PUBLIC_CLEAR_COLOR_PROBE=' + ('ON' if args.clear_color_check else 'OFF'),
        '-DNVN_CLEAR_COLOR_PATTERN_LIBRARY=' + clear_color_library,
        '-DNVN_PUBLIC_COPY_CALL=' + ('ON' if args.public_copy_call else 'OFF'),
        '-DNVN_PUBLIC_VOLUME_COPY_PROBE=' + ('ON' if args.public_volume_copy_check else 'OFF'),
        '-DNVN_PUBLIC_VOLUME_COPY_HEADER=' + public_volume_copy_header,
        '-DNVN_PUBLIC_COPY_PROBE=' + ('ON' if args.clear_color_check or args.public_copy_check or args.public_packaged_copy_check else 'OFF'),
        '-DNVN_PUBLIC_COPY_HEADER=' + (clear_color_header or public_packaged_copy_header or public_copy_header),
        '-DNVN_PUBLIC_PACKAGED_COPY_EXPERIMENT=' + ('ON'if args.public_packaged_copy_check else'OFF'),
        '-DNVN_BLOCK_COPY_PROBE=' + ('ON' if args.block_copy_check else 'OFF'),
        '-DNVN_BLOCK_COPY_HEADER=' + block_header,
        '-DNVN_TEXTURE_CONVERSION_PROBE=' + ('ON' if args.texture_conversion_check else 'OFF'),
        '-DNVN_TEXTURE_CONVERSION_HEADER=' + conversion_header,
        '-DNVN_NVN_PIPELINE_CALL=' + ('ON' if args.nvn_pipeline_call else 'OFF'),
        '-DNVN_CASC_GATHER_DEPTH_PROBE=' + ('ON' if args.casc_gather_depth_check else 'OFF'),
        '-DNVN_CASC_GATHER_DEPTH_LIBRARIES=' + casc_depth_libraries,
        '-DNVN_NVN_PIPELINE_SAMPLED=' + ('ON' if args.nvn_pipeline_sampled_check else 'OFF'),
        '-DNVN_NVN_PIPELINE_PROBE=' + ('ON' if args.casc_gather_depth_check or args.nvn_pipeline_sampled_check or args.nvn_color_target_check or args.nvn_pipeline_buffer_check or args.nvn_pipeline_runtime_views_check or args.nvn_pipeline_check or args.nvn_pipeline_gob_check or args.nvn_pipeline_bc_check or args.nvn_pipeline_bc_mips_check else 'OFF'),
        '-DNVN_NVN_PIPELINE_QUALIFICATION_HEADER=' + nvn_pipeline_header,
        '-DNVN_NVN_PIPELINE_COLOR=' + ('ON' if args.nvn_color_target_check else 'OFF'),
        '-DNVN_NVN_PIPELINE_BUFFER=' + ('ON' if args.nvn_pipeline_buffer_check else 'OFF'),
        '-DNVN_NVN_PIPELINE_BUFFER_FDL_LIBRARY=' + nvn_pipeline_buffer_library,
        '-DNVN_NVN_PIPELINE_BC=' + ('ON' if args.nvn_pipeline_bc_check or args.nvn_pipeline_bc_mips_check else 'OFF'),
        '-DNVN_NVN_PIPELINE_BC_MIPS=' + ('ON' if args.nvn_pipeline_bc_mips_check else 'OFF'),
        '-DNVN_NVN_PIPELINE_BC_FDL_LIBRARY=' + nvn_pipeline_bc_library,
        '-DNVN_NVN_PIPELINE_BC_SRGB=' + ('ON'if args.nvn_pipeline_bc_check and nvn_pipeline_check.get('srgb')is True else'OFF'),
        '-DNVN_NVN_PIPELINE_GOB=' + ('ON' if args.nvn_pipeline_gob_check or (args.nvn_pipeline_runtime_views_check and nvn_pipeline_check['source_layout']=='gob') else 'OFF'),
        '-DNVN_NVN_PIPELINE_RUNTIME_VIEWS=' + ('ON' if args.casc_gather_depth_check or args.nvn_pipeline_sampled_check or args.nvn_color_target_check or args.nvn_pipeline_runtime_views_check else 'OFF'),
        '-DNVN_NVN_PIPELINE_RUNTIME_FDL_LIBRARY=' + nvn_pipeline_runtime_library,
        '-DNVN_NATIVE_PIPELINE_PROBE=' + ('ON' if args.native_front_facing_check or args.native_depth_sampled_check or args.native_depth_shadow_check or args.native_pipeline_check or args.native_color_target_check or args.native_gather_check or args.native_depth_check or args.native_sampler_mip_check else 'OFF'),
        '-DNVN_NATIVE_FRONT_FACING_PROBE=' + ('ON'if args.native_front_facing_check else'OFF'),
        '-DNVN_NATIVE_DEPTH_PROBE=' + ('ON'if args.native_front_facing_check or args.native_depth_sampled_check or args.native_depth_shadow_check or args.native_depth_check else'OFF'),
        '-DNVN_NATIVE_DEPTH_SAMPLED_PROBE=' + ('ON'if args.native_depth_sampled_check else'OFF'),
        '-DNVN_NATIVE_DEPTH_SHADOW_PROBE=' + ('ON'if args.native_depth_shadow_check else'OFF'),
        '-DNVN_NATIVE_DEPTH_LIBRARIES=' + pipeline_depth_libraries,
        '-DNVN_NATIVE_GATHER_PROBE=' + ('ON'if args.native_gather_check else'OFF'),
        '-DNVN_NATIVE_SAMPLER_MIP_PROBE=' + ('ON'if args.native_sampler_mip_check else'OFF'),
        '-DNVN_NATIVE_SAMPLER_MIP_FDL_LIBRARY=' + pipeline_mip_library,
        '-DNVN_NATIVE_COLOR_TARGET_PROBE=' + ('ON' if args.native_color_target_check else 'OFF'),
        '-DNVN_NATIVE_COLOR_TARGET_FDL_LIBRARY=' + pipeline_color_library,
        '-DNVN_PIPELINE_QUALIFICATION_HEADER=' + pipeline_header,
        '-DNVN_TEXTURE_BRIDGE_PROBE=' + ('ON' if args.nvn_texture_bridge_check else 'OFF'),
        '-DNVN_TEXTURE_BRIDGE_DIAGNOSTICS=' + ('ON' if args.texture_bridge_diagnostics else 'OFF'),
        '-DNVN_TEXTURE_BRIDGE_HEADER=' + bridge_header,
        '-DNVN_SAMPLER_PROBE=' + ('ON' if args.nvn_sampler_check else 'OFF'),
        '-DNVN_COUNTER_PROBE=' + ('ON' if args.nvn_counter_check else 'OFF'),
        '-DNVN_COPY_PROBE=' + ('ON' if args.nvn_copy_check else 'OFF'),
        '-DNVN_COPY_PM4_HEADER=' + copy_pm4_header,
        '-DNVN_DEVICE_INSPECT=' + ('ON' if args.device_inspect else 'OFF'),
        '-DNVN_IOCOHERENT_INSPECT=' + ('ON' if args.iocoherent_inspect else 'OFF'),
        '-DNVN_AHB_PROBE=' + ('ON' if args.ahb_inspect else 'OFF'),
        '-DNVN_AHB_PRESENT=' + ('ON' if args.ahb_present_check else 'OFF'),
        '-DNVN_API_PROBE=' + ('ON' if args.nvn_api else 'OFF'),
        f'-DCMAKE_TOOLCHAIN_FILE={ndk}/build/cmake/android.toolchain.cmake',
        '-DANDROID_ABI=arm64-v8a', '-DANDROID_PLATFORM=android-29',
        '-DCMAKE_BUILD_TYPE=RelWithDebInfo')
    run('cmake', '--build', out / 'cmake')
    manifest = root / 'android/AndroidManifest.xml'
    activity = 'android.app.NativeActivity'
    dex = None
    if args.ahb_present_check or args.nvn_window_check or args.fullsize_display_check or args.window_source_slots:
        activity = 'org.nvn.nativeprobe.PresentActivity'
        manifest = out / 'AndroidManifest.xml'
        manifest.write_text((root / 'android/AndroidManifest.xml').read_text()
            .replace('android:hasCode="false"', 'android:hasCode="true"')
            .replace('android:name="android.app.NativeActivity"', f'android:name="{activity}"'))
        classes, dex_dir = out / 'java-classes', out / 'java-dex'
        classes.mkdir(exist_ok=True)
        dex_dir.mkdir(exist_ok=True)
        run('javac', '-source', '8', '-target', '8', '-classpath', sdk / 'platforms/android-36/android.jar',
            '-d', classes, root / 'android/java/org/nvn/nativeprobe/PresentActivity.java')
        run(bt / 'd8', '--min-api', '29', '--lib', sdk / 'platforms/android-36/android.jar',
            '--output', dex_dir, classes / 'org/nvn/nativeprobe/PresentActivity.class')
        dex = dex_dir / 'classes.dex'
    raw, aligned, apk = [out / name for name in ('unsigned.apk', 'aligned.apk', 'probe.apk')]
    run(bt / 'aapt2', 'link', '-I', sdk / 'platforms/android-36/android.jar',
        '--manifest', manifest, '-o', raw)
    library = out / 'cmake/libnvn_kgsl_probe.so'
    with zipfile.ZipFile(raw, 'a', compression=zipfile.ZIP_DEFLATED) as archive:
        archive.write(library, 'lib/arm64-v8a/libnvn_kgsl_probe.so')
        if dex:
            archive.write(dex, 'classes.dex')
        if d2r_host:
            archive.write(d2r_host['executable'], 'lib/arm64-v8a/libnvn_d2r_host.so')
    run(bt / 'zipalign', '-f', '4', raw, aligned)
    key = args.keystore.resolve() if args.keystore else out / 'probe-debug.keystore'
    if not key.exists():
        run('keytool', '-genkeypair', '-keystore', key, '-storepass', 'android',
            '-keypass', 'android', '-alias', 'probe', '-dname', 'CN=NVN local probe',
            '-keyalg', 'RSA', '-validity', '3650')
    run(bt / 'apksigner', 'sign', '--ks', key, '--ks-pass', 'pass:android',
        '--out', apk, aligned)
    run(bt / 'apksigner', 'verify', apk)
    if source_map(root, out) != source or {str(p): sha(p) for p in tool_paths} != tools:
        parser.error('source/tool changed during build; no deployable build record produced')
    record = {'schema': 1, 'source': source, 'commands': commands,
              'shader_check': shader, 'shader_case': args.cs_case if shader else None, 'nvn_api': args.nvn_api,
              'compute_call':args.compute_call,
              'compute_indirect':args.compute_indirect,'compute_indirect_validation':compute_indirect_manifest,
              'maxwell_check': maxwell, 'graphics_check': graphics, 'compiled_inputs': bundle_hashes,
              'graphics_draw_batch': args.graphics_draw_batch,
              'device_inspect': args.device_inspect, 'nvn_graphics_check': graphics_api if args.nvn_graphics_check else None, 'texture_check': texture,
              'nvn_external_image_check': graphics_api if args.nvn_external_image_check else None,
              'external_image_slots': external_slots,
              'nvn_window_check': graphics_api if args.nvn_window_check else None,
              'window_slots': window_slots,
              'window_source_check':window_source_check,'window_source_slots':args.window_source_slots,'fullsize_display_check':fullsize_check,'fullsize_display_slots':fullsize_slots,
              'nvn_counter_check': graphics_api if args.nvn_counter_check else None,
              'nvn_copy_check': copy_check,
              'iocoherent_inspect': args.iocoherent_inspect,
              'bc_sampled_check': bc_sampled_check,
              'texture_texels_check': texture_texels_check,
              'volume_texture_check': volume_texture_check,
              'buffer_texture_check': buffer_texture_check,
              'volume_shadow_check': volume_shadow_check,
              'public_volume_copy_check': public_volume_copy_check,
              'clear_color_check': clear_color_check,
              'public_copy_check': public_copy_check,
              'public_copy_call': args.public_copy_call,
              'public_packaged_copy_check': public_packaged_copy_check,
              'block_copy_check': block_check,
              'texture_conversion_check': conversion_check,
              'native_pipeline_check':pipeline_check if args.native_pipeline_check else None,
              'native_front_facing_check':pipeline_check if args.native_front_facing_check else None,
              'native_depth_check':pipeline_check if args.native_depth_check else None,
              'native_depth_sampled_check':pipeline_check if args.native_depth_sampled_check else None,
              'native_depth_shadow_check':pipeline_check if args.native_depth_shadow_check else None,
              'native_gather_check':pipeline_check if args.native_gather_check else None,
              'native_sampler_mip_check':pipeline_check if args.native_sampler_mip_check else None,
              'native_color_target_check':pipeline_check if args.native_color_target_check else None,
              'nvn_pipeline_runtime_views_check':nvn_pipeline_check if args.nvn_pipeline_runtime_views_check else None,
              'nvn_pipeline_check':nvn_pipeline_check if args.nvn_pipeline_check else None,
              'nvn_pipeline_call':args.nvn_pipeline_call,
              'casc_gather_depth_check':nvn_pipeline_check if args.casc_gather_depth_check else None,
              'nvn_color_target_check':nvn_pipeline_check if args.nvn_color_target_check else None,
              'nvn_pipeline_buffer_check':nvn_pipeline_check if args.nvn_pipeline_buffer_check else None,
              'nvn_pipeline_bc_check':nvn_pipeline_check if args.nvn_pipeline_bc_check else None,
              'nvn_pipeline_sampled_check':nvn_pipeline_check if args.nvn_pipeline_sampled_check else None,
              'nvn_pipeline_bc_mips_check':nvn_pipeline_check if args.nvn_pipeline_bc_mips_check else None,
              'nvn_pipeline_gob_check':nvn_pipeline_check if args.nvn_pipeline_gob_check else None,
              'nvn_texture_bridge_check':bridge_check,
              'texture_bridge_diagnostics':args.texture_bridge_diagnostics,
              'nvn_sampler_check': texture_api if args.nvn_sampler_check else None,
              'nvn_texture_check': texture_api if args.nvn_texture_check else None, 'texture_device_flags': args.texture_device_flags,
              'texture_noaccess_pool': args.texture_noaccess_pool,
              'texture_modes': args.texture_modes,
              'texture_uncached_pools': args.texture_uncached_pools,
              'texture_descriptor_flags': args.texture_descriptor_flags or 0,
              'nvn_contract': native_contract(root),
              'ahb_inspect': args.ahb_inspect,
              'ahb_graphics_check': ahb_graphics if args.ahb_graphics_check else None,
              'ahb_present_check': ahb_graphics if args.ahb_present_check else None,
              'activity': activity, 'd2r_host': d2r_host,
              'tools': tools,
              'artifacts': {str(p): sha(p) for p in (library, apk)},
              'compute_driverparams_check':driverparams_check,
              'scope': 'Standalone native KGSL qualification; no real ARM64 game module execution'}
    (out / 'build-record.json').write_text(json.dumps(record, indent=2) + '\n')
    print(apk)


if __name__ == '__main__':
    main()
