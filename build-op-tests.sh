#! /bin/sh
#
# Build modified versions of the crypto module for FIPS operational
# and failure tests.
#
# FORCE_HMAC_FAIL      - Fail HMAC algorithm self-test.
# FORCE_SIGVER_FAIL    - Fail signature verification self-test.
# FORCE_INTEGRITY_FAIL - Fail with integrity checksum mismatch.
# FORCE_ZEROIZE_FAIL   - Check and fail zeroization.
# ZEROIZE_CHECK        - Check and log success of zeroization.

tests="FORCE_HMAC_FAIL FORCE_SIGVER_FAIL FORCE_INTEGRITY_FAIL \
       FORCE_ZEROIZE_FAIL ZEROIZE_CHECK"

for test in $tests; do
    echo ===== building $test =====

    # Remove built crypto modules
    rm -rf build/uefi*64/crypto

    # Rebuild crypto modules with special define(s)
    make DEBUG=1 CRYPTOTEST=$test

    # Save them
    mkdir -p $test
    mv build/uefi*64/crypto/crypto_*.efi-test_sb2017 $test
done

# Tar up the modules
rm -f build/fips-op-tests.tgz
tar czf build/fips-op-tests.tgz $tests

# Clean up
rm -rf build/uefi*64/crypto
rm -rf $tests
