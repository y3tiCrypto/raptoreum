package=libcurl
$(package)_version=8.6.0
$(package)_download_path=https://curl.se/download/
$(package)_file_name=curl-$($(package)_version).tar.xz
$(package)_sha256_hash=3ccd55d91af9516539df80625f818c734dc6f2ecf9bada33c76765e99121db15
$(package)_dependencies=openssl

define $(package)_set_vars
  $(package)_config_opts=--disable-shared --with-openssl=$(host_prefix) --without-libidn2 --without-brotli --without-zstd --without-libpsl --without-zlib --disable-ldap --disable-ldaps --disable-rtsp --disable-dict --disable-telnet --disable-tftp --disable-pop3 --disable-imap --disable-smb --disable-smtp --disable-gopher --disable-manual --disable-shared
  $(package)_config_opts_linux=--with-pic
  $(package)_config_opts_android=--with-pic
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
