# Monero Ordinals

Copyright (c) 2014-2022 The Monero Project.  
Copyright (c) 2023 The Monero Ordinals Project(mordinals.org).  
Portions Copyright (c) 2012-2013 The Cryptonote developers.

Monero, like any blockchain project, is based on the principle of an open decentralized ledger that guarantees consensus regarding transaction history. However, unlike classic projects like Bitcoin and Ethereum, Monero hides sensitive information from third-party observers, such as transaction participants' addresses, transaction amounts, and the connections between transactions.

Initially, Monero was created as a system with fungible coins, and it is generally considered more fungible than classic blockchain projects due to its private qualities.

The phenomenon of ordinals, which has gained popularity recently, synthetically overcomes this property by associating certain data with transaction outputs, turning them into a digital asset that resembles an NFT.

Implementing ordinals in Monero seemed like an interesting and challenging idea to us. Challenging because the technology on which Monero is built is more complex, and implementing ordinals in the privacy protocol is quite an adventure. Interesting because Monero is perhaps the most community-driven project of all, same time having a very diverse community. We believe that there will be many people who will appreciate our efforts.


## Technical details
An "ordinal" is considered to be the output of a transaction (the 0th output) that contains special data (tx_extra_ordinal_register in the extra field) describing the fact that the output registers an ordinal, as well as storing any graphical and metadata associated with that ordinal. This data is open and transparent in transaction, so an ordinal explorer (https://mordinals.org) can use it to create a table of ordinals. The owner of the wallet to which the transaction output belongs is the owner of the ordinal. As is known, in the Monero blockchain, it is not possible to determine the address based on the transaction output, so we added a meta_info field where the owner can provide their contact information (if desired) or any other information.
In order for such an output to be an ordinal and not just lifeless information sealed in the blockchain, we have defined a protocol for transferring the ordinal to a new owner, such that the ordinal explorer can track this. This protocol should allow for the tracking of the actual transition of the ordinal to another transaction (to the new owner), as opposed to using this output as a decoy. To do this, we have created several outputs that obviously burn coins (and could never be actually spent) - outputs on a zero key, such as in this transaction: https://localmonero.co/blocks/search/35ccad6e5f36a4320d1296ecb02ee34ce1591096658f236915943d2e55e43007 
If only such outputs are specified in the set of decoys, then the ordinal explorer can confidently determine that a transition of the ordinal has occurred in a new transaction. This small tweak to the Monero protocol should not harm the overall privacy of the network, but allows us to organize the transfer of ordinals between community members.

## HOWTO Mint
To mint and control your inscriptions, you will need to use our open-source wallet. Our wallet is a fork of Monero with a number of modifications that allow us to have ordinal protocol on top of the Monero blockchain. Here are a few simple steps that you need to follow to mint your ordinals:

[1] Build the CLI wallet (targeting simplewallet in make) and the daemon. Detailed instructions on how to build the wallet can be found here: 

`https://github.com/mooonero/mordinals#compiling-mordinals-from-source`


[2]. Launch the daemon and wait until it is synchronized. You can download the Monero blockchain from https://www.getmonero.org/downloads/#blockchain to speed up the synchronization process. Alternatively, you can copy the blockchain from any other synchronized Monero node (just make sure to stop it first).
 Don't use pruning. Alternatively, you can use the wallet with a remote node.

[3]. Generate a new wallet using the command line. Remember to backup your seed phrase, as this is mandatory.

`./monero-wallet-cli --generate-new-wallet=/home/wallets/my_mordinals.xmr`


[4]. Open the wallet and synchronize it with the daemon.

`./monero-wallet-cli --wallet-file=/home/wallets/my_mordinals.xmr`


[5]. Tip this address with some funding (10-20$ equivalent should be enough) and wait for the required confirmations. Check that balance is confirmed be "balance" command:

```
[wallet 31yfs3]: balance
Currently selected account: [0] Primary account
Tag: (No tag assigned)
Balance: 0.121121000000, unlocked balance: 0.121121000000
```


[6]. Prepare a .png image that you would like to use as your inscription, and create a .txt file where you can include some short text associated with your ordinal. Note that the text file can be left empty.


[7]. Launch simplewallet and pass the following command to it:

`[wallet 31yfs3]:  mint_ordinal 0.00001 /home/images/my_inscription.png /home/description.txt`


[8]. Wait until the transaction is confirmed, and then check mordinals.org to see if your inscription has been recognized and registered.


[9]. Bask in the glory of being inscribed in Monero's history!

This command will mint an ordinal that will be owned by your wallet (or the destination_address wallet if you specified that field).
**IMPORTANT!!!!** Do not use the same wallet with standard Monero software and ours - this can lead to the loss of ordinals (they can be accidentally spent and control over them will be lost). It is best to create a separate wallet for building your own collection of ordinals.

Check out newly registered ordinals on mordinals.org, or view them on your own monerod using the included `ordinals_viewer.html` file.

## Compiling Mordinals from source

### Dependencies

The following table summarizes the tools and libraries required to build. A
few of the libraries are also included in this repository (marked as
"Vendored"). By default, the build uses the library installed on the system
and ignores the vendored sources. However, if no library is found installed on
the system, then the vendored source will be built and used. The vendored
sources are also used for statically-linked builds because distribution
packages often include only shared library binaries (`.so`) but not static
library archives (`.a`).

| Dep          | Min. version  | Vendored | Debian/Ubuntu pkg    | Arch pkg     | Void pkg           | Fedora pkg          | Optional | Purpose         |
| ------------ | ------------- | -------- | -------------------- | ------------ | ------------------ | ------------------- | -------- | --------------- |
| GCC          | 5             | NO       | `build-essential`    | `base-devel` | `base-devel`       | `gcc`               | NO       |                 |
| CMake        | 3.5           | NO       | `cmake`              | `cmake`      | `cmake`            | `cmake`             | NO       |                 |
| pkg-config   | any           | NO       | `pkg-config`         | `base-devel` | `base-devel`       | `pkgconf`           | NO       |                 |
| Boost        | 1.58          | NO       | `libboost-all-dev`   | `boost`      | `boost-devel`      | `boost-devel`       | NO       | C++ libraries   |
| OpenSSL      | basically any | NO       | `libssl-dev`         | `openssl`    | `libressl-devel`   | `openssl-devel`     | NO       | sha256 sum      |
| libzmq       | 4.2.0         | NO       | `libzmq3-dev`        | `zeromq`     | `zeromq-devel`     | `zeromq-devel`      | NO       | ZeroMQ library  |
| OpenPGM      | ?             | NO       | `libpgm-dev`         | `libpgm`     |                    | `openpgm-devel`     | NO       | For ZeroMQ      |
| libnorm[2]   | ?             | NO       | `libnorm-dev`        |              |                    |                     | YES      | For ZeroMQ      |
| libunbound   | 1.4.16        | NO       | `libunbound-dev`     | `unbound`    | `unbound-devel`    | `unbound-devel`     | NO       | DNS resolver    |
| libsodium    | ?             | NO       | `libsodium-dev`      | `libsodium`  | `libsodium-devel`  | `libsodium-devel`   | NO       | cryptography    |
| libunwind    | any           | NO       | `libunwind8-dev`     | `libunwind`  | `libunwind-devel`  | `libunwind-devel`   | YES      | Stack traces    |
| liblzma      | any           | NO       | `liblzma-dev`        | `xz`         | `liblzma-devel`    | `xz-devel`          | YES      | For libunwind   |
| libreadline  | 6.3.0         | NO       | `libreadline6-dev`   | `readline`   | `readline-devel`   | `readline-devel`    | YES      | Input editing   |
| expat        | 1.1           | NO       | `libexpat1-dev`      | `expat`      | `expat-devel`      | `expat-devel`       | YES      | XML parsing     |
| GTest        | 1.5           | YES      | `libgtest-dev`[1]    | `gtest`      | `gtest-devel`      | `gtest-devel`       | YES      | Test suite      |
| ccache       | any           | NO       | `ccache`             | `ccache`     | `ccache`           | `ccache`            | YES      | Compil. cache   |
| Doxygen      | any           | NO       | `doxygen`            | `doxygen`    | `doxygen`          | `doxygen`           | YES      | Documentation   |
| Graphviz     | any           | NO       | `graphviz`           | `graphviz`   | `graphviz`         | `graphviz`          | YES      | Documentation   |
| lrelease     | ?             | NO       | `qttools5-dev-tools` | `qt5-tools`  | `qt5-tools`        | `qt5-linguist`      | YES      | Translations    |
| libhidapi    | ?             | NO       | `libhidapi-dev`      | `hidapi`     | `hidapi-devel`     | `hidapi-devel`      | YES      | Hardware wallet |
| libusb       | ?             | NO       | `libusb-1.0-0-dev`   | `libusb`     | `libusb-devel`     | `libusbx-devel`     | YES      | Hardware wallet |
| libprotobuf  | ?             | NO       | `libprotobuf-dev`    | `protobuf`   | `protobuf-devel`   | `protobuf-devel`    | YES      | Hardware wallet |
| protoc       | ?             | NO       | `protobuf-compiler`  | `protobuf`   | `protobuf`         | `protobuf-compiler` | YES      | Hardware wallet |
| libudev      | ?             | NO       | `libudev-dev`        | `systemd`    | `eudev-libudev-devel` | `systemd-devel`  | YES      | Hardware wallet |

[1] On Debian/Ubuntu `libgtest-dev` only includes sources and headers. You must
build the library binary manually. This can be done with the following command `sudo apt-get install libgtest-dev && cd /usr/src/gtest && sudo cmake . && sudo make`
then:

* on Debian:
  `sudo mv libg* /usr/lib/`
* on Ubuntu:
  `sudo mv lib/libg* /usr/lib/`

[2] libnorm-dev is needed if your zmq library was built with libnorm, and not needed otherwise

Install all dependencies at once on Debian/Ubuntu:

```
sudo apt update && sudo apt install build-essential cmake pkg-config libssl-dev libzmq3-dev libunbound-dev libsodium-dev libunwind8-dev liblzma-dev libreadline6-dev libexpat1-dev libpgm-dev qttools5-dev-tools libhidapi-dev libusb-1.0-0-dev libprotobuf-dev protobuf-compiler libudev-dev libboost-chrono-dev libboost-date-time-dev libboost-filesystem-dev libboost-locale-dev libboost-program-options-dev libboost-regex-dev libboost-serialization-dev libboost-system-dev libboost-thread-dev python3 ccache doxygen graphviz
```

Install all dependencies at once on Arch:
```
sudo pacman -Syu --needed base-devel cmake boost openssl zeromq libpgm unbound libsodium libunwind xz readline expat gtest python3 ccache doxygen graphviz qt5-tools hidapi libusb protobuf systemd
```

Install all dependencies at once on Fedora:
```
sudo dnf install gcc gcc-c++ cmake pkgconf boost-devel openssl-devel zeromq-devel openpgm-devel unbound-devel libsodium-devel libunwind-devel xz-devel readline-devel expat-devel gtest-devel ccache doxygen graphviz qt5-linguist hidapi-devel libusbx-devel protobuf-devel protobuf-compiler systemd-devel
```

Install all dependencies at once on openSUSE:

```
sudo zypper ref && sudo zypper in cppzmq-devel libboost_chrono-devel libboost_date_time-devel libboost_filesystem-devel libboost_locale-devel libboost_program_options-devel libboost_regex-devel libboost_serialization-devel libboost_system-devel libboost_thread-devel libexpat-devel libminiupnpc-devel libsodium-devel libunwind-devel unbound-devel cmake doxygen ccache fdupes gcc-c++ libevent-devel libopenssl-devel pkgconf-pkg-config readline-devel xz-devel libqt5-qttools-devel patterns-devel-C-C++-devel_C_C++
```

Install all dependencies at once on macOS with the provided Brewfile:

```
brew update && brew bundle --file=contrib/brew/Brewfile
```

FreeBSD 12.1 one-liner required to build dependencies:

```
pkg install git gmake cmake pkgconf boost-libs libzmq4 libsodium unbound
```

### Cloning the repository

Clone recursively to pull-in needed submodule(s):

```
git clone --recursive https://github.com/mooonero/mordinals
```

If you already have a repo cloned, initialize and update:

```
cd mordinals && git submodule init && git submodule update
```

*Note*: If there are submodule differences between branches, you may need 
to use `git submodule sync && git submodule update` after changing branches
to build successfully.

### Build instructions

You need to make the `simplewallet` and `daemon` targets to use mordinals.

#### On Linux

* Install the dependencies
* Change to the root of the source code directory, change to the most recent release branch, and build:

    ```bash
    cd mordinals
    make cmake-release
    cd build/Linux/master/release
    make simplewallet daemon
    ```

* The resulting executables can be found in `build/Linux/master/release/bin`

* Add `PATH="$PATH:$HOME/mordinals/build/Linux/master/release/bin"` to `.profile`

* Run Monero with `monerod`

Dependencies need to be built with -fPIC. Static libraries usually aren't, so you may have to build them yourself with -fPIC. Refer to their documentation for how to build them.
    
## Running monerod

The build places the binary in `bin/` sub-directory within the build directory
from which cmake was invoked (repository root by default). To run in the
foreground:

```bash
./bin/monerod
```

To list all available options, run `./bin/monerod --help`.  Options can be
specified either on the command line or in a configuration file passed by the
`--config-file` argument.  To specify an option in the configuration file, add
a line with the syntax `argumentname=value`, where `argumentname` is the name
of the argument without the leading dashes, for example, `log-level=1`.

To run in background:

```bash
./bin/monerod --log-file monerod.log --detach
```

To run as a systemd service, copy
[monerod.service](utils/systemd/monerod.service) to `/etc/systemd/system/` and
[monerod.conf](utils/conf/monerod.conf) to `/etc/`. The [example
service](utils/systemd/monerod.service) assumes that the user `monero` exists
and its home is the data directory specified in the [example
config](utils/conf/monerod.conf).

If you're on Mac, you may need to add the `--max-concurrency 1` option to
monero-wallet-cli, and possibly monerod, if you get crashes refreshing.

## Internationalization

See [README.i18n.md](docs/README.i18n.md).

## Using Tor

> There is a new, still experimental, [integration with Tor](docs/ANONYMITY_NETWORKS.md). The
> feature allows connecting over IPv4 and Tor simultaneously - IPv4 is used for
> relaying blocks and relaying transactions received by peers whereas Tor is
> used solely for relaying transactions received over local RPC. This provides
> privacy and better protection against surrounding node (sybil) attacks.

While Monero isn't made to integrate with Tor, it can be used wrapped with torsocks, by
setting the following configuration parameters and environment variables:

* `--p2p-bind-ip 127.0.0.1` on the command line or `p2p-bind-ip=127.0.0.1` in
  monerod.conf to disable listening for connections on external interfaces.
* `--no-igd` on the command line or `no-igd=1` in monerod.conf to disable IGD
  (UPnP port forwarding negotiation), which is pointless with Tor.
* `DNS_PUBLIC=tcp` or `DNS_PUBLIC=tcp://x.x.x.x` where x.x.x.x is the IP of the
  desired DNS server, for DNS requests to go over TCP, so that they are routed
  through Tor. When IP is not specified, monerod uses the default list of
  servers defined in [src/common/dns_utils.cpp](src/common/dns_utils.cpp).
* `TORSOCKS_ALLOW_INBOUND=1` to tell torsocks to allow monerod to bind to interfaces
   to accept connections from the wallet. On some Linux systems, torsocks
   allows binding to localhost by default, so setting this variable is only
   necessary to allow binding to local LAN/VPN interfaces to allow wallets to
   connect from remote hosts. On other systems, it may be needed for local wallets
   as well.
* Do NOT pass `--detach` when running through torsocks with systemd, (see
  [utils/systemd/monerod.service](utils/systemd/monerod.service) for details).
* If you use the wallet with a Tor daemon via the loopback IP (eg, 127.0.0.1:9050),
  then use `--untrusted-daemon` unless it is your own hidden service.

Example command line to start monerod through Tor:

```bash
DNS_PUBLIC=tcp torsocks monerod --p2p-bind-ip 127.0.0.1 --no-igd
```

A helper script is in contrib/tor/monero-over-tor.sh. It assumes Tor is installed
already, and runs Tor and Monero with the right configuration.

### Using Tor on Tails

TAILS ships with a very restrictive set of firewall rules. Therefore, you need
to add a rule to allow this connection too, in addition to telling torsocks to
allow inbound connections. Full example:

```bash
sudo iptables -I OUTPUT 2 -p tcp -d 127.0.0.1 -m tcp --dport 18081 -j ACCEPT
DNS_PUBLIC=tcp torsocks ./monerod --p2p-bind-ip 127.0.0.1 --no-igd --rpc-bind-ip 127.0.0.1 \
    --data-dir /home/amnesia/Persistent/your/directory/to/the/blockchain
```

## Debugging

This section contains general instructions for debugging failed installs or problems encountered with Monero. First, ensure you are running the latest version built from the GitHub repo.

### Obtaining stack traces and core dumps on Unix systems

We generally use the tool `gdb` (GNU debugger) to provide stack trace functionality, and `ulimit` to provide core dumps in builds which crash or segfault.

* To use `gdb` in order to obtain a stack trace for a build that has stalled:

Run the build.

Once it stalls, enter the following command:

```bash
gdb /path/to/monerod `pidof monerod`
```

Type `thread apply all bt` within gdb in order to obtain the stack trace

* If however the core dumps or segfaults:

Enter `ulimit -c unlimited` on the command line to enable unlimited filesizes for core dumps

Enter `echo core | sudo tee /proc/sys/kernel/core_pattern` to stop cores from being hijacked by other tools

Run the build.

When it terminates with an output along the lines of "Segmentation fault (core dumped)", there should be a core dump file in the same directory as monerod. It may be named just `core`, or `core.xxxx` with numbers appended.

You can now analyse this core dump with `gdb` as follows:

```bash
gdb /path/to/monerod /path/to/dumpfile`
```

Print the stack trace with `bt`

 * If a program crashed and cores are managed by systemd, the following can also get a stack trace for that crash:

```bash
coredumpctl -1 gdb
```

#### To run Monero within gdb:

Type `gdb /path/to/monerod`

Pass command-line options with `--args` followed by the relevant arguments

Type `run` to run monerod

### Analysing memory corruption

There are two tools available:

#### ASAN

Configure Monero with the -D SANITIZE=ON cmake flag, eg:

```bash
cd build/debug && cmake -D SANITIZE=ON -D CMAKE_BUILD_TYPE=Debug ../..
```

You can then run the monero tools normally. Performance will typically halve.

#### valgrind

Install valgrind and run as `valgrind /path/to/monerod`. It will be very slow.

### LMDB

Instructions for debugging suspected blockchain corruption as per @HYC

There is an `mdb_stat` command in the LMDB source that can print statistics about the database but it's not routinely built. This can be built with the following command:

```bash
cd ~/monero/external/db_drivers/liblmdb && make
```

The output of `mdb_stat -ea <path to blockchain dir>` will indicate inconsistencies in the blocks, block_heights and block_info table.

The output of `mdb_dump -s blocks <path to blockchain dir>` and `mdb_dump -s block_info <path to blockchain dir>` is useful for indicating whether blocks and block_info contain the same keys.

These records are dumped as hex data, where the first line is the key and the second line is the data.
