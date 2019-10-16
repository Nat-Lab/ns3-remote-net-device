# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

# def options(opt):
#     pass

# def configure(conf):
#     conf.check_nonfatal(header_name='stdint.h', define_name='HAVE_STDINT_H')

def build(bld):
    module = bld.create_ns3_module('remote-net-device', ['core'])
    module.source = [
        'model/remote-net-device.cc'
        ]

    module_test = bld.create_ns3_module_test_library('remote-net-device')
    module_test.source = [
        'test/remote-net-device-test-suite.cc',
        ]

    headers = bld(features='ns3header')
    headers.module = 'remote-net-device'
    headers.source = [
        'model/remote-net-device.h'
        ]

    # bld.ns3_python_bindings()

