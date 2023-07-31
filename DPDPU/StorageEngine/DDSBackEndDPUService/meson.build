project('DDSFileBackEnd', 'C', 'CPP',
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

# Resolve irrelevant compiler warnings
add_project_arguments('-Wno-format-zero-length', language: languages)
add_project_arguments('-Wno-address-of-packed-member', language: languages)
add_project_arguments('-Wno-deprecated-declarations', language: languages)

base_app_dependencies = []
base_app_dependencies += dependency('threads')

base_app_inc_dirs = [
        include_directories('Include/'),
]

APP_NAME = 'DDSFileBackEnd'

app_dependencies = base_app_dependencies
app_inc_dirs = base_app_inc_dirs
app_srcs = []

src = 'Source/'

app_srcs += [
        src + 'FileBackEnd.c',
]

executable(APP_NAME,
        app_srcs,
        c_args : base_c_args,
        cpp_args : base_cpp_args,
        dependencies : [app_dependencies],
        include_directories : app_inc_dirs,
        install: install_apps
)
