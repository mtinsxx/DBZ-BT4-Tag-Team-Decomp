#!/usr/bin/env python3
"""Cross-compile the native TLS/WebSocket dependencies, never host libraries."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[2]


def run(*cmd, **kw):
    subprocess.run(list(map(str, cmd)), check=True, **kw)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--prefix', type=Path, default=Path(os.environ['BT4_ANDROID_PREFIX']))
    parser.add_argument('--build', type=Path, default=ROOT / 'build/android-dependencies')
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    build, prefix = args.build.resolve(), args.prefix.resolve()
    build.mkdir(parents=True, exist_ok=True)
    ndk = Path(os.environ['ANDROID_NDK_HOME'])
    tools = ndk / 'toolchains/llvm/prebuilt/linux-x86_64/bin'
    for item in json.loads((ROOT / 'android/dependencies.lock.json').read_text()):
        archive = build / item['filename']
        if not archive.exists():
            run('curl', '-fsSL', '--retry', '3', item['url'], '-o', archive)
        with archive.open('rb') as source:
            if hashlib.file_digest(source, 'sha256').hexdigest() != item['sha256']:
                raise RuntimeError(f'Checksum mismatch: {archive}')
        directory = build / item['filename'].split('.tar.')[0]
        if not directory.exists():
            with tarfile.open(archive) as source: source.extractall(build, filter='data')
    ssl_build = build / 'openssl-build'
    ssl_build.mkdir(exist_ok=True)
    env = dict(os.environ, ANDROID_NDK_ROOT=str(ndk), PATH=str(tools) + ':' + os.environ['PATH'])
    if not (prefix / 'lib/libssl.a').exists():
        run('perl', build / 'openssl-3.5.7/Configure', 'android-arm64', '-D__ANDROID_API__=28',
            'no-shared', 'no-tests', 'no-apps', 'no-module', '-fPIC',
            f'--prefix={prefix}', '--libdir=lib', cwd=ssl_build, env=env)
        run('make', f'-j{args.jobs}', cwd=ssl_build, env=env)
        run('make', 'install_sw', cwd=ssl_build, env=env)
    curl_build = build / 'curl-build'
    run('cmake', '-S', build / 'curl-8.23.0', '-B', curl_build, '-G', 'Ninja',
        f'-DCMAKE_TOOLCHAIN_FILE={ndk}/build/cmake/android.toolchain.cmake',
        '-DANDROID_ABI=arm64-v8a', '-DANDROID_PLATFORM=android-28',
        f'-DCMAKE_INSTALL_PREFIX={prefix}', '-DCMAKE_BUILD_TYPE=Release',
        '-DCMAKE_POSITION_INDEPENDENT_CODE=ON', '-DBUILD_SHARED_LIBS=OFF',
        '-DBUILD_CURL_EXE=OFF', '-DBUILD_TESTING=OFF', '-DBUILD_LIBCURL_DOCS=OFF',
        '-DCURL_USE_OPENSSL=ON', '-DCURL_DISABLE_WEBSOCKETS=OFF',
        '-DCURL_USE_LIBPSL=OFF', '-DUSE_LIBIDN2=OFF', '-DCURL_BROTLI=OFF', '-DCURL_ZSTD=OFF',
        '-DCURL_USE_LIBSSH2=OFF', '-DCURL_USE_LIBSSH=OFF', '-DCURL_USE_GSSAPI=OFF',
        '-DCURL_DISABLE_LDAP=ON', '-DCURL_DISABLE_LDAPS=ON',
        f'-DOPENSSL_INCLUDE_DIR={prefix}/include', f'-DOPENSSL_SSL_LIBRARY={prefix}/lib/libssl.a',
        f'-DOPENSSL_CRYPTO_LIBRARY={prefix}/lib/libcrypto.a')
    run('cmake', '--build', curl_build, '--parallel', args.jobs)
    run('cmake', '--install', curl_build)


if __name__ == '__main__': main()
