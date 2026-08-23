{
  "name": "sample",
  "toolchain": "auto",
  "arch": "arm64-darwin",
  "groups": {
    "Sources": {
      "files": ["counter.c", "hello.c", "projectile.c"],
      "toolchain": "cc1"
    },
    "Engine": {
      "files": ["table.cpp", "vector3.cpp", "smart.cpp"],
      "toolchain": "c++"
    },
    "Headers": {
      "files": ["counter.h", "table.h", "vector3.h"]
    },
    "Shalimar": {
      "files": ["gcd.shl", "primes.shl", "rotmat.shl"],
      "toolchain": "shc"
    }
  },
  "build": {
    "target": "sample",
    "groups": ["Sources", "Engine"]
  }
}
