project('DDSMain', 'C', 'CPP',
        version: '0.1',
        license: 'Proprietary',
        default_options: ['buildtype=release'],
        meson_version: '>= 0.61.2'
)

languages = ['c', 'cpp']

install_apps = false

base_cpp_args = [
        '-std=c++11',
]

base_c_args = [
    '-flax-vector-conversions',
]

##
# Comment this line to restore warnings of experimental DOCA features
##
add_project_arguments('-D DOCA_ALLOW_EXPERIMENTAL_API', language: languages)

##
# Ensure mlnx-dpdk will manage to find our libbsd
##
add_project_arguments('-D RTE_USE_LIBBSD', language: languages)

# Resolve irrelevant compiler warnings
add_project_arguments('-Wno-format-zero-length', language: languages)
add_project_arguments('-Wno-address-of-packed-member', language: languages)
add_project_arguments('-Wno-deprecated-declarations', language: languages)

##
# DPDK & SPDK paths
##
spdk_inc_path = get_option('SpdkInc')
spdk_lib_path = get_option('SpdkLib')
dpdk_lib_path = get_option('DpdkLib')

message('SPDK include path =', spdk_inc_path)
message('SPDK library path =', spdk_lib_path)
message('DPDK library path =', dpdk_lib_path)

base_app_dependencies = []
base_app_dependencies += dependency('threads')
base_app_dependencies += dependency('json-c')
base_app_dependencies += dependency('libbsd')
base_app_dependencies += dependency('libdpdk')

##
# Build
##
common_path = '../Common/'
network_engine_path = '../NetworkEngine/'
doca_path = '/opt/mellanox/doca/applications/common/src'
tldk_path = network_engine_path + 'TLDK/'
offload_engine_path = '../OffloadEngine/DPU/'
storage_engine_path = '../StorageEngine/DDSBackEndDPUService/'

base_app_inc_dirs = [
    include_directories(common_path + 'Include/'),
    include_directories(common_path + 'Include/DPU'),
    include_directories(doca_path),
    include_directories(network_engine_path + 'Include'),
    include_directories(tldk_path + 'libtle_timer'),
    include_directories(tldk_path + 'libtle_misc'),
    include_directories(tldk_path + 'libtle_memtank'),
    include_directories(tldk_path + 'libtle_dring'),
    include_directories(tldk_path + 'libtle_l4p'),
    include_directories(offload_engine_path + 'Include'),
    include_directories(storage_engine_path + 'Include'),
    include_directories(spdk_inc_path),
]

APP_NAME = 'DDSMain'

app_inc_dirs = base_app_inc_dirs
app_srcs = []

app_srcs += [
    'Source/Main.c',
	common_path + 'Source/DPU/BackEndControl.c',
    common_path + 'Source/DPU/CacheTable.c',
    common_path + 'Source/DPU/FileService.c',
    common_path + 'Source/DPU/RingBufferPolling.c',
    network_engine_path + 'Source/DDSBOWPipeline.c',
    network_engine_path + 'Source/DDSBOWCore.c',
    network_engine_path + 'Source/DDSTrafficDirecting.c',
    network_engine_path + 'Source/ActionRedirectPackets.c',
    network_engine_path + 'Source/ActionModifyHeaders.c',
    network_engine_path + 'Source/PEPOLinuxTCP.c',
    network_engine_path + 'Source/PEPOTLDKTCP.c',
    tldk_path + 'libtle_timer/timer.c',
    tldk_path + 'libtle_memtank/memtank.c',
    tldk_path + 'libtle_memtank/misc.c',
    tldk_path + 'libtle_dring/dring.c',
    tldk_path + 'libtle_l4p/ctx.c',
    tldk_path + 'libtle_l4p/event.c',
    tldk_path + 'libtle_l4p/stream_table.c',
    tldk_path + 'libtle_l4p/tcp_ofo.c',
    tldk_path + 'libtle_l4p/tcp_rxtx.c',
    tldk_path + 'libtle_l4p/tcp_stream.c',
    tldk_path + 'libtle_l4p/udp_rxtx.c',
    tldk_path + 'libtle_l4p/udp_stream.c',
    doca_path + '/dpdk_utils.c',
    doca_path + '/offload_rules.c',
    doca_path + '/utils.c',
    offload_engine_path + 'Source/UDF.c',
    storage_engine_path + 'Source/FileBackEnd.c',
    storage_engine_path + 'Source/bdev.c',
    storage_engine_path + 'Source/ControlPlaneHandlers.c',
    storage_engine_path + 'Source/DataPlaneHandlers.c',
    storage_engine_path + 'Source/DPUBackEndDir.c',
    storage_engine_path + 'Source/DPUBackEndFile.c',
    storage_engine_path + 'Source/DPUBackEndStorage.c',
    storage_engine_path + 'Source/Zmalloc.c'
]

##
# Link
##
app_link_args = ['-O3', '-Wl,--no-as-needed']
app_link_args += ['-L' + spdk_lib_path, '-L' + dpdk_lib_path, '-lspdk_bdev_malloc', '-lspdk_bdev', '-lspdk_notify', '-lspdk_bdev_error', '-lspdk_bdev_gpt', '-lspdk_bdev_split', '-lspdk_bdev_delay', '-lspdk_bdev_zone_block', '-lspdk_accel', '-lspdk_accel_ioat', '-lspdk_thread', '-lspdk_trace', '-lspdk_rpc', '-lspdk_jsonrpc', '-lspdk_json', '-lspdk_util', '-lspdk_ioat', '-lspdk_dma', '-lspdk_log', '-lspdk_event', '-lspdk_env_dpdk_rpc', '-lspdk_event_bdev', '-lspdk_init', '-lisal']
app_link_args += ['-lspdk_env_dpdk', '-lspdk_util', '-lspdk_log', '-lrte_eal', '-lrte_mempool', '-lrte_ring', '-lrte_mbuf', '-lrte_bus_pci', '-lrte_pci', '-lrte_mempool_ring', '-lrte_telemetry', '-lrte_kvargs', '-lrte_rcu', '-lrte_power', '-lrte_ethdev', '-lrte_vhost', '-lrte_net', '-lrte_cryptodev', '-lrte_hash']
app_link_args += ['-lrt', '-luuid', '-lssl', '-lcrypto', '-lm', '-lbsd', '-lnuma', '-ldl']
app_link_args += ['-libverbs', '-lrdmacm']


##
# Dependencies
##
app_dependencies = base_app_dependencies
app_dependencies += dependency('doca-argp')
app_dependencies += dependency('doca-flow')
app_dependencies += dependency('doca-common')

dl_dep = meson.get_compiler('c').find_library('dl', required : true)

##
# Final executable
##
executable(
    APP_NAME,
    app_srcs,
    c_args : base_c_args,
    cpp_args : base_cpp_args,
    dependencies : [app_dependencies, dl_dep],
    include_directories : app_inc_dirs,
    link_args : app_link_args,
    install: install_apps,
    build_rpath: spdk_lib_path + ':' + dpdk_lib_path
)
