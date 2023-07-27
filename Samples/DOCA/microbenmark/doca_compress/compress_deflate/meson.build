#
# Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES, ALL RIGHTS RESERVED.
#
# This software product is a proprietary product of NVIDIA CORPORATION &
# AFFILIATES (the "Company") and all right, title, and interest in and to the
# software product, including all associated intellectual property rights, are
# and shall remain exclusively with the Company.
#
# This software product is governed by the End User License Agreement
# provided with the software product.
#

project('DOCA_SAMPLE', 'C', 'CPP',
	# Get version number from file.
	version: run_command(find_program('cat'),
		files('/opt/mellanox/doca/applications/VERSION'), check: true).stdout().strip(),
	license: 'Proprietary',
	default_options: ['buildtype=debug'],
	meson_version: '>= 0.61.2'
)

SAMPLE_NAME = 'compress_deflate'

# Comment this line to restore warnings of experimental DOCA features
add_project_arguments('-D DOCA_ALLOW_EXPERIMENTAL_API', language: ['c', 'cpp'])

sample_dependencies = []
# Required for all DOCA programs
sample_dependencies += dependency('doca-common')
# Library needed for this sample
sample_dependencies += dependency('doca-compress')
# Library used by the main() function
sample_dependencies += dependency('doca-argp')

sample_srcs = [
	# The sample itself
	SAMPLE_NAME + '_sample.c',
	# Main function for the sample's executable
	SAMPLE_NAME + '_main.c',
	# Common code for all DOCA samples
	'/opt/mellanox/doca/samples/common.c',
	# Common code for all DOCA applications
	'/opt/mellanox/doca/applications/common/src/utils.c',
]

sample_inc_dirs  = []
# Common DOCA logic (samples)
sample_inc_dirs += include_directories('/opt/mellanox/doca/samples')
# Common DOCA logic
sample_inc_dirs += include_directories('/opt/mellanox/doca')
# Common DOCA logic (applications)
sample_inc_dirs += include_directories('/opt/mellanox/doca/applications/common/src')


executable('doca_' + SAMPLE_NAME, sample_srcs,
	dependencies : sample_dependencies,
	include_directories: sample_inc_dirs,
	install: false)
