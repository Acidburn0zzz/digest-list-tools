/*
 * Copyright (C) 2017,2018 Huawei Technologies Duesseldorf GmbH
 *
 * Author: Roberto Sassu <roberto.sassu@huawei.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * File: verify_digest_lists.c
 *      Verify digest list metadata and digest lists
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>

#include "securityfs.h"
#include "lib.h"

#define CURRENT_DIR "/etc/ima/digest_lists"

static int verify_list_metadata(char *path, u8 *digest)
{
	void *data, *datap;
	loff_t size, mmap_size, cur_size = 0;
	int ret, fd;

	fd = read_file_from_path(path, &data, &size);
	if (fd < 0) {
		pr_err("Unable to read: %s (%d)\n", path, fd);
		return fd;
	}

	mmap_size = size;

	ret = check_digest(data, size, NULL, ima_hash_algo, digest);
	if (ret < 0) {
		pr_err("Metadata digest mismatch\n");
		goto out;
	}

	datap = data;
	while (size > 0) {
		cur_size = ima_parse_digest_list_metadata(size, datap);
		if (cur_size < 0) {
			ret = -EINVAL;
			goto out;
		}

		size -= cur_size;
		datap += cur_size;
	}
out:
	munmap(data, mmap_size);
	close(fd);
	return ret;
}

static void usage(char *progname)
{
	printf("Usage: %s <options>\n", progname);
	printf("Options:\n");
	printf("\t-d: directory containing metadata and digest lists\n"
	       "\t-m <file name>: metadata file name\n"
	       "\t-i <digest>: expected digest of metadata\n"
	       "\t-h: display help\n"
	       "\t-e <algorithm>: digest algorithm\n");
}

int main(int argc, char *argv[])
{
	int c, ret = -EINVAL;
	u8 input_digest[SHA512_DIGEST_SIZE];
	char *cur_dir = CURRENT_DIR;
	char *metadata_filename = "metadata";
	int digest_len;
	char *digest_ptr = NULL;

	while ((c = getopt(argc, argv, "d:m:i:he:")) != -1) {
		switch (c) {
		case 'd':
			cur_dir = optarg;
			break;
		case 'm':
			metadata_filename = optarg;
			break;
		case 'i':
			digest_ptr = optarg;
			break;
		case 'h':
			usage(argv[0]);
			return -EINVAL;
		case 'e':
			if (ima_hash_setup(optarg)) {
				printf("Unknown algorithm %s\n", optarg);
				return -EINVAL;
			}
			break;
		default:
			printf("Unknown option %c\n", optopt);
			return -EINVAL;
		}
	}

	if (digest_ptr == NULL) {
		printf("Expected metadata digest not specified\n");
		return -EINVAL;
	}

	OpenSSL_add_all_digests();

	digest_len = hash_digest_size[ima_hash_algo];
	hex2bin(input_digest, digest_ptr, digest_len);

	digest_lists_dir_path = cur_dir;

	ret = verify_list_metadata(metadata_filename, input_digest);
	if (ret == 0)
		printf("digest_lists: %d, digests: %d\n",
		       digest_lists, digests);

	EVP_cleanup();
	return ret;
}
