#!/bin/sh
# dev/setup-bend.sh
# Points sole-comb at the pinned Bend 2 checkout of dev/toolchain.json (the
# attest checkout, read only). It never clones, updates or builds Bend: the
# user owns every install and every network step.
# It refuses a checkout whose HEAD is not the pinned revision, a checkout
# with modified tracked sources, and a binary whose sha256 is not the pin.
set -eu
sole_comb_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
python3 -P "$sole_comb_root/dev/pin-check.py" --only bend
sole_comb_bend=$(python3 -P -c 'import json,os,sys; print(json.load(open(sys.argv[1]))["bend"]["checkout"])' "${SOLE_COMB_TOOLCHAIN:-$sole_comb_root/dev/toolchain.json}")
printf '%s\n' "Bend ready at $sole_comb_bend"
