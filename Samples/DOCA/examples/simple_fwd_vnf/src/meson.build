#
# Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES, ALL RIGHTS RESERVED.
#
# This software product is a proprietary product of NVIDIA CORPORATION &
# AFFILIATES (the "Company") and all right, title, and interest in and to the
# software product, including all associated intellectual property rights, are
# and shall remain exclusively with the Company.
#
# This software product is governed by the End User License Agreement
# provided with the software product.
#

app_dependencies += dependency('doca-argp')
app_dependencies += dependency('doca-flow')
app_dependencies += dependency('doca-common')

app_srcs += [
	APP_NAME + '.c',
	'simple_fwd.c',
	'simple_fwd_control.c',
	'simple_fwd_ft.c',
	'simple_fwd_pkt.c',
	'simple_fwd_port.c',
	'simple_fwd_vnf_core.c',
	common_dir_path + '/dpdk_utils.c',
	common_dir_path + '/offload_rules.c',
	common_dir_path + '/utils.c',
]

executable(DOCA_PREFIX + APP_NAME,
	app_srcs,
	dependencies : app_dependencies,
	include_directories : app_inc_dirs,
	install: install_apps)
