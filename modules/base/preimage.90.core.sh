# Copyright (C) 2009 One Laptop Per Child
# Licensed under the terms of the GNU GPL v2 or later; see COPYING for details.

. $OOB__shlib
versioned_fs=$(read_config base versioned_fs)

buildnr=$(read_buildnr)
isopath=$outputdir/os$buildnr.iso



# due to popular demand
# FIXME: ship in olpc-utils?
cat >$fsmount/boot/README <<EOF
The following documentation is for developers only.


=== PARTITIONED LAYOUT ===
On systems where 2 partitions are used (root and boot), OLPC OS builds
duplicate the boot contents in 2 places:

1. On the root filesystem, at /versions/pristine/hash/boot
   which in turn is hardlinked to /versions/run/hash/boot
   A booted system chroots into the /versions/run/hash root, so these files are
   then accessible at /boot.
   These files are not used during boot.

2. On the boot filesystem, which is mounted on a running system at /bootpart
   This is a versioned filesystem layout.
   OpenFirmware looks at the "boot" directory (a symlink in this case) on this
   partition during early boot in order to find the kernel and supporting
   files.

In other words, if you want to use a different kernel or initramfs then you
want to put the files in /bootpart/boot.

Installing a kernel RPM will put the new kernel and initramfs in /boot, so
this will not take effect on reboot. You need to synchronize those new files
into the real boot partition, e.g.:
	# rsync --delete-before -av /boot/ /bootpart/boot/

This duplication is a consequence of the olpc-update design, where updates
can currently only be presented as a single filesystem tree described by a
single contents manifest. That filesystem tree must remain in pristine
condition in order for future updates to happen efficiently.


=== UNPARTITIONED LAYOUT ===

On systems where only a single partition is used, there is no duplication
of files (although there are multiple links). However, on a booted system,
/boot is not (exactly) the location that OFW boots from.

On a booted system, you are typically not seeing the actual disk layout,
instead you are seeing a chroot (with various bind mounts to distort it
further) setup by the initramfs.

OFW boots from the real /boot, which is an indirect symlink to the real
/versions/pristine/x/boot. On a booted system, /boot corresponds to the
real /versions/run/x/boot. Therefore any files you put in /boot will not
be available to OFW at boot-time, and if you break the hard links of existing
files then your changes will also not take effect.

Installing a kernel RPM will put the new kernel and initramfs in /boot, so
this will not take effect on reboot. You need to synchronize those new files
into the real boot area, e.g.:
	# cp -a /boot/* /versions/boot/current/boot/
EOF


# normalize modification times of all files for faster updates (#4259)
# note that python needs special consideration here: the mtime is encoded
# into the pyc file. so we must regenerate all pyc files.
OLPC_EPOCH="2007-11-02 12:00:00Z"

# kill all *.pyo and *.pyc
find $fsmount/etc -xdev -name "*.py[oc]" -delete
find $fsmount/usr/lib -xdev -name "*.py[oc]" -delete
find $fsmount/usr/share -xdev -name "*.py[oc]" -delete

# note: we ignore errors from "find" below, because touch will try and
# update mtimes on symlinks to (e.g.) /proc/self/fd/0 which will not succeed
# for obvious reasons

# normalize mtimes of all files
echo "Normalize file times..."
find $fsmount -xdev -print0 | xargs -0 touch -c -d "$OLPC_EPOCH" || :

# now regenerate the .pyc files
# (add -OO to generate .pyo files instead when we tackle dlo trac #8431)
echo "Compiling python bytecode..."
chroot $fsmount python -m compileall /usr/lib /usr/share > /dev/null
# now we have to normalize the mtimes of the new pyc/pyo files, but we'll do 
# that after we've finished making other fs changes below

if [ "$versioned_fs" = "1" ]; then
	# Make upgradable
	mkdir -p $fsmount/versions/pristine/$buildnr
	mkdir -p $fsmount/versions/{updates,contents,sticky,run}
	for f in $(ls -a $fsmount/ |egrep -v '^(\.|\.\.|versions|home|security|lost\+found)$' ); do
		mv $fsmount/$f $fsmount/versions/pristine/$buildnr/
	done

	mkdir -p $fsmount/versions/pristine/$buildnr/{versions,home,security}
	mkdir -p $fsmount/{sys,proc,ofw,dev}

	echo "Generating contents manifest..."
	chroot $fsmount/versions/pristine/$buildnr /usr/sbin/olpc-contents-create -f /.xo-files -p /etc/passwd -g /etc/group /
	mv $fsmount/versions/pristine/$buildnr/.xo-files $fsmount/versions/contents/$buildnr
	cp $fsmount/versions/contents/$buildnr $outputdir/os$buildnr.toc
fi

# now normalize mtimes again
echo "Normalize file times..."
find $fsmount -xdev -print0 | xargs -0 touch -c -d "$OLPC_EPOCH" || :

# now that we've generated the .contents file, its important that nobody
# makes any more changes to the files. let's try and be sure of that
mount -o remount,ro $fsmount
