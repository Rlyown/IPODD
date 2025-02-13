/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2005,2006,2007,2008 IBM Corporation
 *
 * Authors:
 * Reiner Sailer <sailer@watson.ibm.com>
 * Mimi Zohar <zohar@us.ibm.com>
 *
 * File: ima.h
 *	internal Integrity Measurement Architecture (IMA) definitions
 */

#ifndef __LINUX_IMA_H
#define __LINUX_IMA_H

#include "linux/list.h"
#include <crypto/hash_info.h>
#include <linux/audit.h>
#include <linux/crypto.h>
#include <linux/fs.h>
#include <linux/hash.h>
#include <linux/security.h>
#include <linux/tpm.h>
#include <linux/types.h>
#ifdef CONFIG_IMA_FPCR
#include <linux/mnt_namespace.h>
#endif /* CONFIG_IMA_FPCR */

#include "../integrity.h"

#ifdef CONFIG_HAVE_IMA_KEXEC
#include <asm/ima.h>
#endif

enum ima_show_type {
	IMA_SHOW_BINARY,
	IMA_SHOW_BINARY_NO_FIELD_LEN,
	IMA_SHOW_BINARY_OLD_STRING_FMT,
	IMA_SHOW_ASCII
};
enum tpm_pcrs { TPM_PCR0 = 0, TPM_PCR8 = 8, TPM_PCR10 = 10 };

/* digest size for IMA, fits SHA1 or MD5 */
#define IMA_DIGEST_SIZE SHA1_DIGEST_SIZE
#define IMA_EVENT_NAME_LEN_MAX 255

#define IMA_HASH_BITS 10
#define IMA_MEASURE_HTABLE_SIZE (1 << IMA_HASH_BITS)

#define IMA_TEMPLATE_FIELD_ID_MAX_LEN 16
#define IMA_TEMPLATE_NUM_FIELDS_MAX 15

#define IMA_TEMPLATE_IMA_NAME "ima"
#define IMA_TEMPLATE_IMA_FMT "d|n"

/* current content of the policy */
extern int ima_policy_flag;

/* set during initialization */
extern int ima_hash_algo;
extern int ima_appraise;
extern struct tpm_chip *ima_tpm_chip;
extern const char boot_aggregate_name[];

/* IMA event related data */
struct ima_event_data {
	struct integrity_iint_cache *iint;
	struct file *file;
	const unsigned char *filename;
	struct evm_ima_xattr_data *xattr_value;
	int xattr_len;
	const struct modsig *modsig;
	const char *violation;
	const void *buf;
	int buf_len;
};

/* IMA template field data definition */
struct ima_field_data {
	u8 *data;
	u32 len;
};

/* IMA template field definition */
struct ima_template_field {
	const char field_id[IMA_TEMPLATE_FIELD_ID_MAX_LEN];
	int (*field_init)(struct ima_event_data *event_data,
			  struct ima_field_data *field_data);
	void (*field_show)(struct seq_file *m, enum ima_show_type show,
			   struct ima_field_data *field_data);
};

/* IMA template descriptor definition */
struct ima_template_desc {
	struct list_head list;
	char *name;
	char *fmt;
	int num_fields;
	const struct ima_template_field **fields;
};

struct ima_template_entry {
	int pcr;
	u8 digest[TPM_DIGEST_SIZE]; /* sha1 or md5 measurement hash */
	struct ima_template_desc *template_desc; /* template descriptor */
	u32 template_data_len;
	struct ima_field_data template_data[0]; /* template related data */
};

struct ima_queue_entry {
	struct hlist_node hnext; /* place in hash collision list */
	struct list_head later; /* place in ima_measurements list */
	struct ima_template_entry *entry;
};
extern struct list_head ima_measurements; /* list of all measurements */

/* Some details preceding the binary serialized measurement list */
struct ima_kexec_hdr {
	u16 version;
	u16 _reserved0;
	u32 _reserved1;
	u64 buffer_size;
	u64 count;
};

extern const int read_idmap[];

#ifdef CONFIG_HAVE_IMA_KEXEC
void ima_load_kexec_buffer(void);
#else
static inline void ima_load_kexec_buffer(void)
{
}
#endif /* CONFIG_HAVE_IMA_KEXEC */

/*
 * The default binary_runtime_measurements list format is defined as the
 * platform native format.  The canonical format is defined as little-endian.
 */
extern bool ima_canonical_fmt;

/* Internal IMA function definitions */
int ima_init(void);
int ima_fs_init(void);
int ima_add_template_entry(struct ima_template_entry *entry, int violation,
			   const char *op, struct inode *inode,
			   const unsigned char *filename);
#ifdef CONFIG_IMA_FPCR
int ima_fpcr_add_template_entry(struct ima_template_entry *entry, int violation,
				const char *op, struct inode *inode,
				const unsigned char *filename,
				unsigned int fpcr_id);
#endif /* CONFIG_IMA_FPCR */
int ima_calc_file_hash(struct file *file, struct ima_digest_data *hash);
int ima_calc_buffer_hash(const void *buf, loff_t len,
			 struct ima_digest_data *hash);
int ima_calc_field_array_hash(struct ima_field_data *field_data,
			      struct ima_template_desc *desc, int num_fields,
			      struct ima_digest_data *hash);
int ima_calc_boot_aggregate(struct ima_digest_data *hash);
void ima_add_violation(struct file *file, const unsigned char *filename,
		       struct integrity_iint_cache *iint, const char *op,
		       const char *cause);
int ima_init_crypto(void);
void ima_putc(struct seq_file *m, void *data, int datalen);
void ima_print_digest(struct seq_file *m, u8 *digest, u32 size);
int template_desc_init_fields(const char *template_fmt,
			      const struct ima_template_field ***fields,
			      int *num_fields);
struct ima_template_desc *ima_template_desc_current(void);
struct ima_template_desc *lookup_template_desc(const char *name);
bool ima_template_has_modsig(const struct ima_template_desc *ima_template);
int ima_restore_measurement_entry(struct ima_template_entry *entry);
int ima_restore_measurement_list(loff_t bufsize, void *buf);
int ima_measurements_show(struct seq_file *m, void *v);
unsigned long ima_get_binary_runtime_size(void);
int ima_init_template(void);
void ima_init_template_list(void);
int __init ima_init_digests(void);
int ima_lsm_policy_change(struct notifier_block *nb, unsigned long event,
			  void *lsm_data);

/*
 * used to protect h_table and sha_table
 */
extern spinlock_t ima_queue_lock;

struct ima_h_table {
	atomic_long_t len; /* number of stored measurements in the list */
	atomic_long_t violations;
	struct hlist_head queue[IMA_MEASURE_HTABLE_SIZE];
};
extern struct ima_h_table ima_htable;

static inline unsigned int ima_hash_key(u8 *digest)
{
	/* there is no point in taking a hash of part of a digest */
	return (digest[0] | digest[1] << 8) % IMA_MEASURE_HTABLE_SIZE;
}

#define __ima_hooks(hook)                                                      \
	hook(NONE) hook(FILE_CHECK) hook(MMAP_CHECK) hook(BPRM_CHECK)          \
		hook(CREDS_CHECK) hook(POST_SETATTR) hook(MODULE_CHECK)        \
			hook(FIRMWARE_CHECK) hook(KEXEC_KERNEL_CHECK)          \
				hook(KEXEC_INITRAMFS_CHECK) hook(POLICY_CHECK) \
					hook(KEXEC_CMDLINE) hook(MAX_CHECK)
#define __ima_hook_enumify(ENUM) ENUM,

enum ima_hooks { __ima_hooks(__ima_hook_enumify) };

extern const char *const func_tokens[];

struct modsig;

/* LIM API function definitions */
int ima_get_action(struct inode *inode, const struct cred *cred, u32 secid,
		   int mask, enum ima_hooks func, int *pcr,
		   struct ima_template_desc **template_desc);
int ima_must_measure(struct inode *inode, int mask, enum ima_hooks func);
int ima_collect_measurement(struct integrity_iint_cache *iint,
			    struct file *file, void *buf, loff_t size,
			    enum hash_algo algo, struct modsig *modsig);
void ima_store_measurement(struct integrity_iint_cache *iint, struct file *file,
			   const unsigned char *filename,
			   struct evm_ima_xattr_data *xattr_value,
			   int xattr_len, const struct modsig *modsig, int pcr,
			   struct ima_template_desc *template_desc);
#ifdef CONFIG_IMA_FPCR
void ima_fpcr_store_measurement(
	struct integrity_iint_cache *iint, struct file *file,
	const unsigned char *filename, struct evm_ima_xattr_data *xattr_value,
	int xattr_len, const struct modsig *modsig, int pcr,
	struct ima_template_desc *template_desc, unsigned int fpcr_id);
#endif /* CONFIG_IMA_FPCR */

void ima_audit_measurement(struct integrity_iint_cache *iint,
			   const unsigned char *filename);
int ima_alloc_init_template(struct ima_event_data *event_data,
			    struct ima_template_entry **entry,
			    struct ima_template_desc *template_desc);
int ima_store_template(struct ima_template_entry *entry, int violation,
		       struct inode *inode, const unsigned char *filename,
		       int pcr);
#ifdef CONFIG_IMA_FPCR
int ima_fpcr_store_template(struct ima_template_entry *entry, int violation,
			    struct inode *inode, const unsigned char *filename,
			    int pcr, unsigned int fpcr_id);
#endif /* CONFIG_IMA_FPCR */
void ima_free_template_entry(struct ima_template_entry *entry);
const char *ima_d_path(const struct path *path, char **pathbuf, char *filename);

/* IMA policy related functions */
int ima_match_policy(struct inode *inode, const struct cred *cred, u32 secid,
		     enum ima_hooks func, int mask, int flags, int *pcr,
		     struct ima_template_desc **template_desc);
void ima_init_policy(void);
void ima_update_policy(void);
void ima_update_policy_flag(void);
ssize_t ima_parse_add_rule(char *);
void ima_delete_rules(void);
int ima_check_policy(void);
void *ima_policy_start(struct seq_file *m, loff_t *pos);
void *ima_policy_next(struct seq_file *m, void *v, loff_t *pos);
void ima_policy_stop(struct seq_file *m, void *v);
int ima_policy_show(struct seq_file *m, void *v);

/* Appraise integrity measurements */
#define IMA_APPRAISE_ENFORCE 0x01
#define IMA_APPRAISE_FIX 0x02
#define IMA_APPRAISE_LOG 0x04
#define IMA_APPRAISE_MODULES 0x08
#define IMA_APPRAISE_FIRMWARE 0x10
#define IMA_APPRAISE_POLICY 0x20
#define IMA_APPRAISE_KEXEC 0x40

#ifdef CONFIG_IMA_APPRAISE
int ima_appraise_measurement(enum ima_hooks func,
			     struct integrity_iint_cache *iint,
			     struct file *file, const unsigned char *filename,
			     struct evm_ima_xattr_data *xattr_value,
			     int xattr_len, const struct modsig *modsig);
int ima_must_appraise(struct inode *inode, int mask, enum ima_hooks func);
void ima_update_xattr(struct integrity_iint_cache *iint, struct file *file);
enum integrity_status ima_get_cache_status(struct integrity_iint_cache *iint,
					   enum ima_hooks func);
enum hash_algo ima_get_hash_algo(struct evm_ima_xattr_data *xattr_value,
				 int xattr_len);
int ima_read_xattr(struct dentry *dentry,
		   struct evm_ima_xattr_data **xattr_value);

#else
static inline int
ima_appraise_measurement(enum ima_hooks func, struct integrity_iint_cache *iint,
			 struct file *file, const unsigned char *filename,
			 struct evm_ima_xattr_data *xattr_value, int xattr_len,
			 const struct modsig *modsig)
{
	return INTEGRITY_UNKNOWN;
}

static inline int ima_must_appraise(struct inode *inode, int mask,
				    enum ima_hooks func)
{
	return 0;
}

static inline void ima_update_xattr(struct integrity_iint_cache *iint,
				    struct file *file)
{
}

static inline enum integrity_status
ima_get_cache_status(struct integrity_iint_cache *iint, enum ima_hooks func)
{
	return INTEGRITY_UNKNOWN;
}

static inline enum hash_algo
ima_get_hash_algo(struct evm_ima_xattr_data *xattr_value, int xattr_len)
{
	return ima_hash_algo;
}

static inline int ima_read_xattr(struct dentry *dentry,
				 struct evm_ima_xattr_data **xattr_value)
{
	return 0;
}

#endif /* CONFIG_IMA_APPRAISE */

#ifdef CONFIG_IMA_APPRAISE_MODSIG
bool ima_hook_supports_modsig(enum ima_hooks func);
int ima_read_modsig(enum ima_hooks func, const void *buf, loff_t buf_len,
		    struct modsig **modsig);
void ima_collect_modsig(struct modsig *modsig, const void *buf, loff_t size);
int ima_get_modsig_digest(const struct modsig *modsig, enum hash_algo *algo,
			  const u8 **digest, u32 *digest_size);
int ima_get_raw_modsig(const struct modsig *modsig, const void **data,
		       u32 *data_len);
void ima_free_modsig(struct modsig *modsig);
#else
static inline bool ima_hook_supports_modsig(enum ima_hooks func)
{
	return false;
}

static inline int ima_read_modsig(enum ima_hooks func, const void *buf,
				  loff_t buf_len, struct modsig **modsig)
{
	return -EOPNOTSUPP;
}

static inline void ima_collect_modsig(struct modsig *modsig, const void *buf,
				      loff_t size)
{
}

static inline int ima_get_modsig_digest(const struct modsig *modsig,
					enum hash_algo *algo, const u8 **digest,
					u32 *digest_size)
{
	return -EOPNOTSUPP;
}

static inline int ima_get_raw_modsig(const struct modsig *modsig,
				     const void **data, u32 *data_len)
{
	return -EOPNOTSUPP;
}

static inline void ima_free_modsig(struct modsig *modsig)
{
}
#endif /* CONFIG_IMA_APPRAISE_MODSIG */

/* LSM based policy rules require audit */
#ifdef CONFIG_IMA_LSM_RULES

#define security_filter_rule_init security_audit_rule_init
#define security_filter_rule_free security_audit_rule_free
#define security_filter_rule_match security_audit_rule_match

#else

static inline int security_filter_rule_init(u32 field, u32 op, char *rulestr,
					    void **lsmrule)
{
	return -EINVAL;
}

static inline void security_filter_rule_free(void *lsmrule)
{
}

static inline int security_filter_rule_match(u32 secid, u32 field, u32 op,
					     void *lsmrule)
{
	return -EINVAL;
}
#endif /* CONFIG_IMA_LSM_RULES */

#ifdef CONFIG_IMA_READ_POLICY
#define POLICY_FILE_FLAGS (S_IWUSR | S_IRUSR)
#else
#define POLICY_FILE_FLAGS S_IWUSR
#endif /* CONFIG_IMA_READ_POLICY */

#ifdef CONFIG_IMA_FPCR
#define FPCR_NULL_ID 0
/* extern int FPCR_NULL_ID_DELAY; */
#define FPCR_DATA_SIZE TPM_DIGEST_SIZE

#define MERKLE_TREE_DATA_SIZE FPCR_DATA_SIZE

static inline int binary_upper(int num)
{
	if (num <= 0)
		return 1;
	if ((num & (num - 1)) == 0)
		return num;
	num |= num >> 1;
	num |= num >> 2;
	num |= num >> 4;
	num |= num >> 8;
	num |= num >> 16;
	return num + 1;
}

// Merkle Tree 结构体定义
struct merkle_tree_node {
	int used; // 有效标识位
	int random;
	unsigned char data[MERKLE_TREE_DATA_SIZE];
};

// #define MERKLE_TREE_SIZE ((1 << 22) / binary_upper(sizeof(struct merkle_tree_node)))
#define MERKLE_TREE_SIZE (64)

struct merkle_tree {
	int id;
	int last_empty;
	struct crypto_shash *tfm;
	struct list_head list;
	struct merkle_tree_node node_list[0];
};

extern int merkle_tree_init(struct merkle_tree **mt);
extern int merkle_tree_get_empty(struct merkle_tree *mt);
extern int merkle_tree_update(struct merkle_tree *mt, int node, const u8 *data);
extern int merkle_tree_extend(struct merkle_tree *mt, int node, const u8 *data);

#define merkle_tree_node_data(mt, node) ((mt)->node_list[node].data)
#define merkle_tree_root_data(mt) merkle_tree_node_data(mt, 1)
#define merkle_tree_isfull(mt) ((mt)->last_empty >= MERKLE_TREE_SIZE)
#define merkle_tree_id(mt) ((mt)->id)

extern struct list_head merkle_tree_list;
extern struct merkle_tree *merkle_tree_list_get_empty(void);

// MMAP标签是一个分界线，大于MMAP的标签不会从struct file获取信息
enum ima_file_label_action {
	LABEL_HOOK_OPEN = 1,
	LABEL_HOOK_READ,
	LABEL_HOOK_WRITE,
	LABEL_HOOK_CLOSE,
	LABEL_HOOK_SYNC,
	LABEL_HOOK_FSETXATTR,
	LABEL_HOOK_FTRUNCATE,
	LABEL_HOOK_LSEEK,
	LABEL_HOOK_FCNTL,
	LABEL_HOOK_MMAP,
	LABEL_HOOK_RENAME,
	LABEL_HOOK_TRUNCATE,
	LABEL_HOOK_FSTAT,
	LABEL_HOOK_UNLINK,
	LABEL_HOOK_LINK,
};

struct ima_file_path {
	char filename[NAME_MAX];
	char *pathbuf;
	const char *pathname;
};

struct ima_file_label {
	uid_t uid;
	pid_t pid;

	struct file *file;
	struct dentry *dentry;
	struct ima_file_path fpath;
	unsigned int fpcr_id;

	enum ima_file_label_action action;

	// point to state
	struct ima_file_state *state;
};

void ima_file_label_init(struct ima_file_label *flabel, struct file *file,
			 struct path *path, enum ima_file_label_action action);
// unsigned int get_fpcr_id_from_file(struct file *file);
unsigned int hash_ima_file_label(struct ima_file_label *flabel);
int ima_file_label_to_string(struct ima_file_label *flabel, char *buf,
			     int max_buf);
/* int ima_fpcr_collect_measurement(struct integrity_iint_cache *iint, */
/*                                  struct file *file, void *buf, loff_t size,
 */
/*                                  enum hash_algo algo, struct modsig *modsig,
 */
/*                                  struct ima_file_label *flabel); */

int ima_file_filter(struct dentry *dentry, unsigned int action);
int ima_fpcr_get_next(struct ima_file_state *state, int ev);
// int add_info_to_file_hash(struct ima_file_label *flabel,
//                           struct ima_digest_data *hash);

// struct ima_fpcr {
//     struct crypto_shash *tfm;
//     unsigned char data[FPCR_DATA_SIZE];
//     unsigned char secret[FPCR_DATA_SIZE];
// };

struct ima_fpcr_history {
	struct crypto_shash *tfm;
	unsigned char data[FPCR_DATA_SIZE];
};

struct fpcr_link_node {
	struct filename *name;
	int dfd;

	struct list_head list;
};

struct ima_file_state {
	// current state
	int state;
	int write_to_sync;
	int load_content;
	int error;
	int finish;
	int start_seq;
	int ready;
};

struct fpcr_list {
	unsigned int id;
	// struct ima_fpcr *fpcr;
	struct dentry *measurement_log;
	struct list_head list; // travel the list
	struct list_head measurements; // global measurements in namespace
	struct list_head link_group; // relative hard link files

	struct ima_file_state state;

	// fpcr
	struct merkle_tree *mt;
	int tree_node_id;
};

extern struct fpcr_list user_id_fpcr_list;

// get fpcr_list node by ns number
struct ima_fpcr_h_table {
	atomic_long_t len; // nums of measurements
	struct hlist_head queue[IMA_MEASURE_HTABLE_SIZE];
};

extern struct ima_fpcr_h_table ima_fpcr_htable;

struct ima_fpcr_h_entry {
	struct hlist_node hnext;
	struct fpcr_list *id_list;
};

static inline unsigned long ima_fpcr_hash_key(unsigned int id)
{
	return hash_long(id, IMA_HASH_BITS);
}

extern int ima_fpcr_invoke_measure(struct ima_file_label *flabel);
extern int ima_create_measurement_log(struct fpcr_list *node);
extern struct fpcr_list *ima_fpcr_lookup_entry(unsigned int id);
extern int ima_fpcr_add_link_node(struct ima_file_label *flabel,
				  struct fpcr_list *node);
extern int ima_get_random(u8 *out, size_t max);
/* extern int ima_is_systemd_ns(unsigned int id); */

/* record the history value of physical PCR to bind all ima_fpcrs into a
 * physical PCR. */
extern struct ima_fpcr_history fpcr_for_history;

int ima_init_fpcr_structures(void);

#endif /* CONFIG_IMA_FPCR */

#endif /* __LINUX_IMA_H */
