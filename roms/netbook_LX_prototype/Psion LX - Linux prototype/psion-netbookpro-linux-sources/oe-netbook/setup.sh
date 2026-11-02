#  env-dev-arm-oe.source script
# customised for netbook project by Robert McQueen <rob@aleph1.co.uk>
# make copies of this script with different build dirs to create multiple trees

# set this to the location of OE
OEHOME=/usr/src/netbook/
OEDIR=${OEHOME}/oe/oe

# set this to the location of OE's packages dir
PKGDIR=${OEHOME}/oe/oe-packages

# set this to the location of the netbook OE tree
NBDIR=${OEHOME}/oe-netbook

# set this to the desired build directory - doesn't need to be the same
BUILDDIR=${NBDIR}

# set this to your username on cvs.psiondev.com
PDUSER=${USER}

## Under here should be Ok - but read it anyway

# Don't export TARGET_ARCH - it *will* cause build failures

# trivial check for incorrect execution
if [ `basename $0` == setup.sh ]; then
  echo This script is designed to be sourced, not executed.
  echo Please type \". setup.sh\" instead.
  exit 1
fi

# check that build dir exists
mkdir -p ${BUILDDIR}/conf || {
  echo Unable to make build directory, exiting.
  exit 1
}

# copy in local.conf if it doesn't exist, so local modifications
# are invisible to CVS
if [ ! -f ${BUILDDIR}/conf/local.conf ]; then
  echo Creating new local.conf file
  cp ${NBDIR}/conf/local.conf.sample ${BUILDDIR}/conf/local.conf
fi

# export OE conffile paths
OEPATH=${NBDIR}:${PKGDIR}:${OEDIR}
export OEPATH

# export locations of .oe files
OEFILES="${NBDIR}/*/*.oe ${PKGDIR}/*/*.oe"
export OEFILES

# export psiondev user and netbook dir (defined above)
export PDUSER NBDIR

# preserve PATH then add the OE bin dir
if [ -z ${ORG_PATH} ]
then
 ORG_PATH=${PATH}
 export ORG_PATH
fi
PATH=$OEDIR/bin:${ORG_PATH}
export PATH

# preserve LD_LIBRARY_PATH then blank it to avoid linking non-arm libaries
if [ -z ${ORG_LD_LIBRARY_PATH} ]
then
 ORG_LD_LIBRARY_PATH=${LD_LIBRARY_PATH}
 export ORG_LD_LIBRARY_PATH
fi
LD_LIBRARY_PATH=
export LD_LIBRARY_PATH

# stop multi byte characters breaking the patcher stuff
# (this is for Redhat / Fedora people really)
export LANG=C

echo "Altered environment for OpenEmbedded"
