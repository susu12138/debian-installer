/* Program to search for disks with a correct yaboot setup, and then
   prompt the user for which one to install yaboot on.

   Copyright (C) 2003 Thorsten Sauter <tsauter@gmx.net>
   Copyright (C) 2002 Colin Walters <walters@gnu.org>
*/

#include "choose-yaboot-disk.h"

void unmount_proc() {
  system("umount /target/proc >>/var/log/messages 2>&1");
}

PedExceptionOption exception_handler(PedException *ex) {
	if(ex->type > 3) {
		fprintf(stderr, "A fatal error occurred: %s\n", ex->message);
		exit(1);
	}
	return(PED_EXCEPTION_UNHANDLED);
}

/* read all physical discs in the system */
int read_physical_discs() {
	DIR *dir = NULL;
	struct dirent *entry = NULL;
	int count = 0;

	if((dir = opendir("/dev/discs")) == NULL) {
		fprintf(stderr, "Failed to open find disks: %s (/dev/discs)\n",
			strerror(errno));
		return(0);
	}

	while((entry = readdir(dir)) != NULL) {
			char *fullname = NULL;
			char lnkname[1024];
			PedDevice *pdev = NULL;
			PedDiskType *ptype = NULL;

			/* skip dot-files */
			if(entry->d_name[0] == '.')
				continue;

			/* create the fullname (inkl. resolve links) */
			asprintf(&fullname, "/dev/discs/%s", entry->d_name);
			realpath(fullname, lnkname);
			free(fullname); fullname = NULL;
			asprintf(&fullname, "%s/%s", lnkname, "disc");

			/* make sure, this disk is usable */
			if((pdev = ped_device_get(fullname)) == NULL) {
				free(fullname);
				continue;
			}
			ptype = ped_disk_probe(pdev);
			if(strcmp(ptype->name, "mac")) {
				free(fullname);
				continue;
			}

			if(read_all_partitions(strdup(fullname)) > 0)
				count++;
			free(fullname);
	}

	closedir(dir);
	return(count);
}

int read_all_partitions(const char *devname) {
	PedDevice *pdev = NULL;
	PedDisk *pdisk = NULL;
	PedPartition *ppart = NULL;
	int count = 0;

	if((pdev = ped_device_get(devname)) == NULL)
		return(0);
	if((pdisk = ped_disk_new(pdev)) == NULL)
		return(0);

	while((ppart = ped_disk_next_partition(pdisk, ppart)) != NULL) {
		if(is_bootable(ppart) == 1) {
			bparts[part_count] = ppart;
			part_count++;
			count++;
		}
	}

	return(count);
}

int is_bootable(const PedPartition *part) {
	if((part->type & PED_PARTITION_METADATA || 
	part->type & PED_PARTITION_FREESPACE)) {
		return(0);
	}

	if(ped_partition_is_flag_available(part, PED_PARTITION_BOOT) &&
	ped_partition_get_flag(part, PED_PARTITION_BOOT)) {
		if(part->geom.length*512 >= (800*1024)) {
        return(1);	/* puh. yes, this disk is yaboot ready :) */
      }
	}

	return(0);
}

char *find_rootpartition() {
	FILE *fmounts = NULL;
	char line[1024];

	fmounts = fopen("/proc/mounts", "r");
	if(fmounts == NULL) {
		fprintf(stderr, "Unable to open /proc/mounts: %s\n", strerror(errno));
		return(NULL);
	}

	while(fgets(line, 1024, fmounts) != NULL) {
		char fs[1024], mnt[1024];

		sscanf(line, "%s %s %*s", fs, mnt);
		if(strcmp(mnt, TARGET) == 0) {
			fclose(fmounts);
			return(strdup(fs));
		}
	}

	fclose(fmounts);
	return(NULL);
}

int generate_yabootconf(const char *boot_devfs, const char *root_devfs) {

  char *dev_ofpath = NULL, *disk = NULL, *partnr = NULL, *cmd = NULL;
  char boot[PATH_MAX], root[PATH_MAX];
  size_t linesize = 0;
  int ret = -1, len = 0;
  FILE *conf = NULL, *ofpath = NULL;

  /* convert paths to non-devfs */
  di_mapdevfs(boot_devfs, boot, PATH_MAX);
  di_mapdevfs(root_devfs, root, PATH_MAX);

  /* split disk device and partitionnr of root partition */ 
  len = strcspn(root, "0123456789");
  partnr = &root[len];
  
  disk = malloc(sizeof(char)*(len+1));
  strncpy(disk, root, len);
  disk[len] = '\0';

  /* get ofpath of disk */
  asprintf(&cmd, "chroot /target /usr/sbin/ofpath %s", disk);
  ofpath = popen(cmd, "r");
  if (ofpath == NULL) {
    printf("popen cmd=%s", cmd);
    return(1);
  }
  ret = getline(&dev_ofpath, &linesize, ofpath);
  if (ret <= 0) {
    return(1);
  }
  pclose(ofpath);


	/* we generate our own yaboot.conf kernel (yabootconfig is horribly broken)*/
	conf = fopen("/target/etc/yaboot.conf", "w");
	if(conf == NULL)
		return(1);
   fprintf(conf, "## yaboot.conf generated by debian-installer\n");
   fprintf(conf, "##\n");
   fprintf(conf, "## run: \"man yaboot.conf\" for details. Do not make changes until you have!!\n");
   fprintf(conf, "## see also: /usr/share/doc/yaboot/examples for example configurations.\n");
   fprintf(conf, "##\n");
   fprintf(conf, "## For a dual-boot menu, add one or more of:\n");
   fprintf(conf, "## bsd=/dev/hdaX, macos=/dev/hdaY, macosx=/dev/hdaZ\n\n");
   fprintf(conf, "boot=%s\n", boot);
   fprintf(conf, "device=%s", dev_ofpath);
   fprintf(conf, "partition=%s\n", partnr);
   fprintf(conf, "root=%s\n", root);
   fprintf(conf, "timeout=30\n");
   fprintf(conf, "install=/usr/lib/yaboot/yaboot\n");
   fprintf(conf, "magicboot=/usr/lib/yaboot/ofboot\n");
 
   fprintf(conf, "image=/vmlinux\n");
   fprintf(conf, "\tlabel=linux\n");
   fprintf(conf, "\tread-only\n");
   /* fprintf(conf, "\tappend = \"video=ofonly\""); FIXME: add this line if booted in safe-mode */

	fprintf(conf, "\n");
	fprintf(conf, "image=/vmlinux.old\n");
	fprintf(conf, "\tlabel=old\n");
	fprintf(conf, "\tread-only\n");
	fprintf(conf, "\n");
	fprintf(conf, "enablecdboot\n");
	fprintf(conf, "\n");
	fclose(conf);

   free(dev_ofpath);
   free(disk);
   free(cmd);

	return(0);
}

int update_kernelconf() {
	FILE *conf = NULL;

	conf = fopen("/target/etc/kernel-img.conf", "w");
	if(conf == NULL)
		return(1);

	fprintf(conf, "## Generated by d-i yaboot-installer\n");
	fprintf(conf, "\n");
	fprintf(conf, "do_symlinks = Yes\n");
	fprintf(conf, "image_in_boot = Yes\n");
	fprintf(conf, "relative_links = Yes\n");
	fprintf(conf, "\n");

	fclose(conf);
	return(0);
}

char *build_choice(PedPartition *part) {
	char *string = NULL;

        asprintf(&string, "%s", ped_partition_get_path(part));
	return(strdup(string));
}

char *extract_choice(const char *choice) {
	char *blank = NULL;
	char device[PATH_MAX];
	int i;

	for(i=0; i<PATH_MAX; i++)
	device[i] = '\0';

	blank = strchr(choice, 32);
	if(blank == NULL) {
		return(strdup(choice));
	}

	strncpy(device, choice, blank-choice);
	return(strdup(device));
}

int main(int argc, char **argv) {
	int i = 0;
	char *rootpart = NULL;
	char *choices = NULL;

	/* initialize debconf */
	debconf = debconfclient_new();
	debconf->command(debconf, "title", "Installing yaboot", NULL);

	/* first, check if this machine as newworld */
   /* FIXME: this is currently broken (needs new subarch detection)	
     if(get_powerpc_type() != 0) {
		debconf->command(debconf, "fset", "yaboot-installer/wrongmac",
			"seen", "false", NULL);
		debconf->command(debconf, "set", "yaboot-installer/wrongmac", "false", NULL);
		debconf->command(debconf, "input", "critical", "yaboot-installer/wrongmac", NULL);
		debconf->command(debconf, "go", NULL);
		exit(1);
      } */

	/* add a libparted exception handler */
	ped_exception_set_handler(exception_handler);

	/* walk through the disks, and store all boot partitions */
	read_physical_discs();
	if(part_count <= 0) {
		debconf->command(debconf, "fset", "yaboot-installer/nopart",
			"seen", "false", NULL);
		debconf->command(debconf, "set", "yaboot-installer/nopart", "false", NULL);
		debconf->command(debconf, "input", "critical", "yaboot-installer/nopart", NULL);
		debconf->command(debconf, "go", NULL);
		exit(1);
	}

	rootpart = find_rootpartition();
	if(rootpart == NULL) {
		debconf->command(debconf, "fset", "yaboot-installer/noroot",
			"seen", "false", NULL);
		debconf->command(debconf, "set", "yaboot-installer/noroot", "false", NULL);
		debconf->command(debconf, "input", "critical", "yaboot-installer/noroot", NULL);
		debconf->command(debconf, "go", NULL);
		exit(1);
	}

	/* build a cdebconf compatible string */
	for(i=0; i<part_count; i++) {
		char *tmp = NULL;

		tmp = build_choice(bparts[i]);
		if(tmp == NULL)
			continue;

		if(choices == NULL)
			asprintf(&choices, "%s", tmp);
		else
			asprintf(&choices, "%s, %s", choices, tmp);

		free(tmp);
	}

	/* ask for boot partition */
	debconf->command(debconf, "subst", "yaboot-installer/bootdev",
		"DEVICES", choices, NULL);
	debconf->command(debconf, "fset", "yaboot-installer/bootdev",
		"seen", "false", NULL);
	debconf->command(debconf, "set", "yaboot-installer/bootdev", "false", NULL); 
	debconf->command(debconf, "input", "critical", "yaboot-installer/bootdev", NULL);
	debconf->command(debconf, "go", NULL);
	debconf->command(debconf, "get", "yaboot-installer/bootdev", NULL);
	if(strcmp(debconf->value, "false") == 0) {
		debconf->command(debconf, "fset", "yaboot-installer/nopart",
			"seen", "false", NULL);
		debconf->command(debconf, "set", "yaboot-installer/nopart", "false", NULL);
		debconf->command(debconf, "input", "critical", "yaboot-installer/nopart", NULL);
		debconf->command(debconf, "go", NULL);
                exit(1);
	}

	/* update the kernel config file */
	i = update_kernelconf();
	if(i != 0) {
		debconf->command(debconf, "fset", "yaboot-installer/kconferr",
			"seen", "false", NULL);
		debconf->command(debconf, "set", "yaboot-installer/kconferr", "false", NULL);
		debconf->command(debconf, "input", "high", "yaboot-installer/kconferr", NULL);
		debconf->command(debconf, "go", NULL);
	}

   /* mkofboot needs proc
      running them in chroot /target */
   i = system("mount -t proc none /target/proc >>/var/log/messages 2>&1");
   atexit((void*)unmount_proc);
   if (i != 0) {
     debconf->command(debconf, "fset", "yaboot-installer/mounterr",
                      "seen", "false", NULL);
     debconf->command(debconf, "set", "yaboot-installer/mounterr", "false", NULL);
     debconf->command(debconf, "input", "critical", "yaboot-installer/mounterr", NULL);
     debconf->command(debconf, "go", NULL);
     exit(1);
   }                
     
	/* generate yaboot.conf */
	i = generate_yabootconf(debconf->value, rootpart);
	if(i != 0) {
		debconf->command(debconf, "fset", "yaboot-installer/conferr",
			"seen", "false", NULL);
		debconf->command(debconf, "set", "yaboot-installer/conferr", "false", NULL);
		debconf->command(debconf, "input", "critical", "yaboot-installer/conferr", NULL);
		debconf->command(debconf, "go", NULL);
                exit(1);
	}
	
	/* running "mkofboot" */
	i = system("chroot /target /usr/sbin/mkofboot -v -f >>/var/log/messages 2>&1");
	if(WEXITSTATUS(i) != 0) {
		debconf->command(debconf, "fset", "yaboot-installer/ybinerr",
			"seen", "false", NULL);
		debconf->command(debconf, "set", "yaboot-installer/ybinerr", "false", NULL);
		debconf->command(debconf, "input", "critical", "yaboot-installer/ybinerr", NULL);
		debconf->command(debconf, "go", NULL);
		return(1);
	}
	
	debconf->command(debconf, "fset", "yaboot-installer/success", "seen", "false", NULL);
	debconf->command(debconf, "set", "yaboot-installer/success", "false", NULL);
	debconf->command(debconf, "input", "medium", "yaboot-installer/success", NULL);
	debconf->command(debconf, "go", NULL);
	return(0);
}

