# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  # This target is included by 'net' target.
  'type': '<(component)',
  'variables': { 'enable_wexit_time_destructors': 1, },
  'dependencies': [
    '../base/base.gyp:base',
    '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
    '../crypto/crypto.gyp:crypto',
    '../sdch/sdch.gyp:sdch',
    '../third_party/boringssl/boringssl.gyp:boringssl',
    '../third_party/protobuf/protobuf.gyp:protobuf_lite',
    '../third_party/zlib/zlib.gyp:zlib',
    '../url/url.gyp:url_url_features',
    'net_derived_sources',
    'net_quic_proto',
    'net_resources',
  ],
  'sources': [
    '<@(net_nacl_common_sources)',
    '<@(net_non_nacl_sources)',
  ],
  'defines': [
    'NET_IMPLEMENTATION',
  ],
  'export_dependent_settings': [
    '../base/base.gyp:base',
    '../third_party/boringssl/boringssl.gyp:boringssl',
  ],
  'conditions': [
    ['chromeos==1', {
      'sources!': [
         'base/network_change_notifier_linux.cc',
         'base/network_change_notifier_linux.h',
         'base/network_change_notifier_netlink_linux.cc',
         'base/network_change_notifier_netlink_linux.h',
         'proxy/proxy_config_service_linux.cc',
         'proxy/proxy_config_service_linux.h',
      ],
    }],
    ['use_kerberos==1', {
      'defines': [
        'USE_KERBEROS',
      ],
      'conditions': [
        ['OS=="openbsd"', {
          'include_dirs': [
            '/usr/include/kerberosV'
          ],
        }],
        ['linux_link_kerberos==1', {
          'link_settings': {
            'ldflags': [
              '<!@(krb5-config --libs gssapi)',
            ],
          },
        }, { # linux_link_kerberos==0
          'defines': [
            'DLOPEN_KERBEROS',
          ],
        }],
      ],
    }, { # use_kerberos == 0
      'sources!': [
        'http/http_auth_gssapi_posix.cc',
        'http/http_auth_gssapi_posix.h',
        'http/http_auth_handler_negotiate.cc',
        'http/http_auth_handler_negotiate.h',
      ],
    }],
    ['posix_avoid_mmap==1', {
      'defines': [
        'POSIX_AVOID_MMAP',
      ],
      'direct_dependent_settings': {
        'defines': [
          'POSIX_AVOID_MMAP',
        ],
      },
      'sources!': [
        'disk_cache/blockfile/mapped_file_posix.cc',
      ],
    }, { # else
      'sources!': [
        'disk_cache/blockfile/mapped_file_avoid_mmap_posix.cc',
      ],
    }],
    ['disable_file_support!=1', {
      # TODO(mmenke):  Should probably get rid of the dependency on
      # net_resources in this case (It's used in net_util, to format
      # directory listings.  Also used outside of net/).
      'sources': ['<@(net_file_support_sources)']
    }],
    ['disable_ftp_support!=1', {
      'sources': ['<@(net_ftp_support_sources)']
    }],
    ['enable_built_in_dns==1', {
      'defines': [
        'ENABLE_BUILT_IN_DNS',
      ]
    }, { # else
      'sources!': [
        'dns/address_sorter_posix.cc',
        'dns/address_sorter_posix.h',
        'dns/dns_client.cc',
      ],
    }],
    [ 'use_nss_certs == 1', {
        'dependencies': [
          '../build/linux/system.gyp:nss',
        ],
      }, {
        'sources!': [
          'cert/x509_util_nss.h',
        ]
      }
    ],
    ['chromecast==1 and use_nss_certs==1', {
      'sources': [
        'ssl/ssl_platform_key_chromecast.cc',
      ],
      'sources!': [
        'ssl/ssl_platform_key_nss.cc',
      ],
    }],
    [ 'use_openssl_certs == 0', {
        'sources!': [
          'base/crypto_module_openssl.cc',
          'base/keygen_handler_openssl.cc',
          'base/openssl_private_key_store.h',
          'base/openssl_private_key_store_android.cc',
          'base/openssl_private_key_store_memory.cc',
          'cert/cert_database_openssl.cc',
          'cert/cert_verify_proc_openssl.cc',
          'cert/cert_verify_proc_openssl.h',
          'cert/test_root_certs_openssl.cc',
          'cert/x509_certificate_openssl.cc',
          'ssl/openssl_client_key_store.cc',
          'ssl/openssl_client_key_store.h',
        ],
    }],
    [ 'use_glib == 1', {
        'dependencies': [
          '../build/linux/system.gyp:gconf',
          '../build/linux/system.gyp:gio',
        ],
    }],
    [ 'desktop_linux == 1 or chromeos == 1', {
        'conditions': [
          ['os_bsd==1', {
            'sources!': [
              'base/network_change_notifier_linux.cc',
              'base/network_change_notifier_netlink_linux.cc',
              'proxy/proxy_config_service_linux.cc',
            ],
          },{
            'dependencies': [
              '../build/linux/system.gyp:libresolv',
            ],
          }],
          ['OS=="solaris"', {
            'link_settings': {
              'ldflags': [
                '-R/usr/lib/mps',
              ],
            },
          }],
        ],
      },
    ],
    [ 'use_nss_certs != 1', {
        'sources!': [
          'base/crypto_module_nss.cc',
          'base/keygen_handler_nss.cc',
          'cert/cert_database_nss.cc',
          'cert/cert_verify_proc_nss.cc',
          'cert/cert_verify_proc_nss.h',
          'cert/nss_cert_database.cc',
          'cert/nss_cert_database.h',
          'cert/nss_cert_database_chromeos.cc',
          'cert/nss_cert_database_chromeos.h',
          'cert/nss_profile_filter_chromeos.cc',
          'cert/nss_profile_filter_chromeos.h',
          'cert/test_root_certs_nss.cc',
          'cert/x509_certificate_nss.cc',
          'cert/x509_util_nss.cc',
          'cert_net/nss_ocsp.cc',
          'cert_net/nss_ocsp.h',
          'ssl/client_cert_store_nss.cc',
          'ssl/client_cert_store_nss.h',
          'ssl/client_key_store.cc',
          'ssl/client_key_store.h',
          'ssl/ssl_platform_key_nss.cc',
          'third_party/mozilla_security_manager/nsKeygenHandler.cpp',
          'third_party/mozilla_security_manager/nsKeygenHandler.h',
          'third_party/mozilla_security_manager/nsNSSCertificateDB.cpp',
          'third_party/mozilla_security_manager/nsNSSCertificateDB.h',
          'third_party/mozilla_security_manager/nsPKCS12Blob.cpp',
          'third_party/mozilla_security_manager/nsPKCS12Blob.h',
        ],
      },
    ],
    [ 'use_nss_certs == 1', {
        'sources': [
          'third_party/nss/ssl/cmpcert.cc',
          'third_party/nss/ssl/cmpcert.h',
        ],
    }],
    [ 'enable_websockets == 1', {
        'defines': ['ENABLE_WEBSOCKETS'],
        'sources': ['<@(net_websockets_sources)']
    }],
    [ 'enable_mdns != 1', {
        'sources!' : [
          'dns/mdns_cache.cc',
          'dns/mdns_cache.h',
          'dns/mdns_client.cc',
          'dns/mdns_client.h',
          'dns/mdns_client_impl.cc',
          'dns/mdns_client_impl.h',
        ]
    }],
    [ 'OS == "win"', {
        'sources!': [
          'http/http_auth_handler_ntlm_portable.cc',
        ],
         # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
        'msvs_disabled_warnings': [4267, ],
        'all_dependent_settings': {
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalDependencies': [
                'crypt32.lib',
                'dhcpcsvc.lib',
                'iphlpapi.lib',
                'rpcrt4.lib',
                'secur32.lib',
                'urlmon.lib',
                'winhttp.lib',
              ],
            },
          },
        },
        'msvs_settings': {
          'VCLinkerTool': {
            'AdditionalDependencies': [
              'crypt32.lib',
              'dhcpcsvc.lib',
              'iphlpapi.lib',
              'rpcrt4.lib',
              'secur32.lib',
              'urlmon.lib',
              'winhttp.lib',
            ],
          },
        },
      }, { # else: OS != "win"
        'sources!': [
          'base/winsock_init.cc',
          'base/winsock_init.h',
          'base/winsock_util.cc',
          'base/winsock_util.h',
          'proxy/proxy_resolver_winhttp.cc',
          'proxy/proxy_resolver_winhttp.h',
        ],
      },
    ],
    [ 'OS == "mac"', {
        'link_settings': {
          'libraries': [
            '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
            '$(SDKROOT)/System/Library/Frameworks/Security.framework',
            '$(SDKROOT)/System/Library/Frameworks/SystemConfiguration.framework',
            '$(SDKROOT)/usr/lib/libresolv.dylib',
          ]
        },
      },
    ],
    [ 'OS == "ios"', {
        'sources!': [
          'disk_cache/blockfile/file_posix.cc',
        ],
        'link_settings': {
          'libraries': [
            '$(SDKROOT)/System/Library/Frameworks/CFNetwork.framework',
            '$(SDKROOT)/System/Library/Frameworks/MobileCoreServices.framework',
            '$(SDKROOT)/System/Library/Frameworks/Security.framework',
            '$(SDKROOT)/System/Library/Frameworks/SystemConfiguration.framework',
          ],
          'xcode_settings': {
            'OTHER_LDFLAGS': [
              '-lresolv',
            ],
          },
        },
      },
    ],
    [ 'OS == "ios" or OS == "mac"', {
        'sources': [
          '<@(net_base_mac_ios_sources)',
        ],
      },
    ],
    ['OS=="android" and _toolset=="target"', {
      'dependencies': [
         'net_java',
      ],
    }],
    [ 'OS == "android"', {
        'dependencies': [
          'net_jni_headers',
        ],
        'sources!': [
          'base/openssl_private_key_store_memory.cc',
          'cert/cert_database_openssl.cc',
          'cert/cert_verify_proc_openssl.cc',
          'cert/test_root_certs_openssl.cc',
          'http/http_auth_gssapi_posix.cc',
          'http/http_auth_gssapi_posix.h',
        ],
      },
    ],
  ],
  'target_conditions': [
    # These source files are excluded by default platform rules, but they
    # are needed in specific cases on other platforms. Re-including them can
    # only be done in target_conditions as it is evaluated after the
    # platform rules.
    ['OS == "android"', {
      'sources/': [
        ['include', '^base/platform_mime_util_linux\\.cc$'],
        ['include', '^base/address_tracker_linux\\.cc$'],
        ['include', '^base/address_tracker_linux\\.h$'],
        ['include', '^base/network_interfaces_linux\\.cc$'],
        ['include', '^base/network_interfaces_linux\\.h$'],
      ],
    }],
    ['OS == "ios"', {
      'sources/': [
        ['include', '^base/mac/url_conversions\\.h$'],
        ['include', '^base/mac/url_conversions\\.mm$'],
        ['include', '^base/network_change_notifier_mac\\.cc$'],
        ['include', '^base/network_config_watcher_mac\\.cc$'],
        ['include', '^base/network_interfaces_mac\\.cc$'],
        ['include', '^base/network_interfaces_mac\\.h$'],
        ['include', '^base/platform_mime_util_mac\\.mm$'],
        ['include', '^proxy/proxy_resolver_mac\\.cc$'],
        ['include', '^proxy/proxy_server_mac\\.cc$'],
      ],
    }],
    ['OS == "ios"', {
      'sources/': [
        ['include', '^cert/test_root_certs_mac\\.cc$'],
      ],
    }],
  ],
}
