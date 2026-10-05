# _hashlib.py — Wii shim: redirects to HACL* built-in hash modules.
# OpenSSL/_hashlib.so is not available on Wii; hashlib.py already falls back
# gracefully, but direct `import _hashlib` would raise ImportError without this.
import _md5 as _m, _sha1 as _s1, _sha2 as _s2, _sha3 as _s3, _blake2 as _b2

# hmac.py does `import _hashlib as _hashopenssl` and, on success, expects
# `_hashopenssl.compare_digest` (only the ImportError branch falls back to
# _operator).  Since this shim makes the import succeed, provide the same
# constant-time comparison here so hmac/urllib3/pip keep working.
from _operator import _compare_digest as compare_digest

# hmac.py evaluates `_hashopenssl.UnsupportedDigestmodError` inside an except
# clause.  If the attribute is absent, the except-clause evaluation itself
# raises AttributeError which propagates uncaught.  Provide the class so
# hmac falls through to its built-in (_hmac / pure-Python) path gracefully.
class UnsupportedDigestmodError(ValueError):
    """Raised when a requested digest algorithm is not supported by OpenSSL."""

# Lowercase alias used in some internal code paths.
unsupportedDigestmodError = UnsupportedDigestmodError

# hmac.py calls hmac_new() and hmac_digest() when _hashopenssl is available.
# Raise UnsupportedDigestmodError so hmac.py falls through to _hmac / pure Python.
def hmac_new(key, msg=b'', digestmod=None):
    raise UnsupportedDigestmodError("OpenSSL HMAC not available on Wii")

def hmac_digest(key, msg, digest):
    raise UnsupportedDigestmodError("OpenSSL hmac_digest not available on Wii")

# HASH / HASHXOF: base types checked with isinstance() in some stdlib code.
class HASH:
    """Stub for OpenSSL HASH base type (not used on Wii)."""
class HASHXOF(HASH):
    """Stub for OpenSSL HASHXOF base type (not used on Wii)."""

_ALGORITHMS = {
    'md5':       _m.md5,
    'sha1':      _s1.sha1,
    'sha224':    _s2.sha224,  'sha256': _s2.sha256,
    'sha384':    _s2.sha384,  'sha512': _s2.sha512,
    'sha3_224':  _s3.sha3_224, 'sha3_256': _s3.sha3_256,
    'sha3_384':  _s3.sha3_384, 'sha3_512': _s3.sha3_512,
    'shake_128': _s3.shake_128, 'shake_256': _s3.shake_256,
    'blake2b':   _b2.blake2b,  'blake2s':   _b2.blake2s,
}

def new(name, data=b'', *, usedforsecurity=True):
    key = name.lower().replace('-', '_')
    fn = _ALGORITHMS.get(key)
    if fn is None:
        raise ValueError(f'unsupported hash type {name}')
    h = fn()
    if data:
        h.update(data)
    return h

def openssl_md5(data=b'', *, usedforsecurity=True):   return new('md5',    data)
def openssl_sha1(data=b'', *, usedforsecurity=True):  return new('sha1',   data)
def openssl_sha256(data=b'', *, usedforsecurity=True): return new('sha256', data)
def openssl_sha512(data=b'', *, usedforsecurity=True): return new('sha512', data)

def pbkdf2_hmac(hash_name, password, salt, iterations, dklen=None):
    import hmac as _hmac
    return _hmac.pbkdf2_hmac(hash_name, password, salt, iterations, dklen)

openssl_md_meth_names = frozenset(_ALGORITHMS)
algorithms_available  = frozenset(_ALGORITHMS)
