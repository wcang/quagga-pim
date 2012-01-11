#include <zebra.h>

#include "thread.h"
#include "prefix.h"

/* PIM message type */
enum pim_type {
  PIM_TYPE_HELLO = 0,
  PIM_TYPE_REGISTER,
  PIM_TYPE_REGISTER_STOP,
  PIM_TYPE_JOIN_PRUNE,
  PIM_TYPE_ASSERT,
  PIM_TYPE_GRAFT,
  PIM_TYPE_GRAFT_ACK,
  PIM_TYPE_CAND_RP_ADV,
  PIM_TYPE_MIN = PIM_TYPE_HELLO,
  PIM_TYPE_MAX = PIM_TYPE_CAND_RP_ADV,
  PIM_TYPE_INVALID = 16
};

/* PIM Hello TLV type */
enum hello_type {
  HELLO_TYPE_HOLDTIME = 1,
  HELLO_TYPE_LAN_PRUNE_DELAY,
  HELLO_TYPE_DR_PRIORITY = 19,
  HELLO_TYPE_GENID,
  HELLO_TYPE_ADDR_LIST = 24
};

/* Flag for Encoded-Source Address */
#define ENC_SRC_FLAG_SPARSE  0x4
#define ENC_SRC_FLAG_WC      0x2  /* Wildcard */
#define ENC_SRC_FLAG_RPT     0x1

/* For Encoded-Group Address and Encoded-Source Address */
struct encoded_addr {
  uint8_t mask;
  struct prefix prefix;
};


struct pim_header {
#if BYTE_ORDER == BIG_ENDIAN    /* non-portable hack that assumes that GCC is used */
  uint8_t version: 4;
  uint8_t type: 4;
#else
  uint8_t type: 4;
  uint8_t version: 4;
#endif
  uint8_t reserved;
  uint16_t checksum;
};

/* PIMv6 encoded unicast address */
struct pim6_enc_uni_addr {
  uint8_t family;
  uint8_t type;
  struct in6_addr address;
};

/* PIMv6 encoded group address */
struct pim6_enc_grp_addr {
  uint8_t family;
  uint8_t type;
#if BYTE_ORDER == BIG_ENDIAN
  uint8_t bidirectional: 1;
  uint8_t reserved: 6;
  uint8_t zone: 1;
#else
  uint8_t zone: 1;
  uint8_t reserved: 6;
  uint8_t bidirectional: 1;
#endif
  uint8_t mask_len;
  struct in6_addr address;
};

/* PIMv6 encoded source address */
struct pim6_enc_src_addr {
  uint8_t family;
  uint8_t type;
#if BYTE_ORDER == BIG_ENDIAN
  uint8_t reserved: 5;
  uint8_t sparse: 1;
  uint8_t wildcard: 1;
  uint8_t rpt: 1;
#else
  uint8_t rpt: 1;
  uint8_t wildcard: 1;
  uint8_t sparse: 1;
  uint8_t reserved: 5;
#endif
  uint8_t mask_len;
  struct in6_addr address;
};

/* PIM Hello TLV */
struct pim_tlv {
  uint16_t type;
  uint16_t length;
  uint8_t value[];
};

#define PIM_VERSION 0x2

/* Address family for Encoded-Unicast Address, Encoded-Group Address and Encoded-Source Address as obtained
 * from http://www.iana.org/assignments/address-family-numbers/address-family-numbers.xml
 */
#define AF_IPV4 1
#define AF_IPV6 2

void init_gen_id(void);

  
int pim6_receive(struct thread *thread);


int pim6_hello_send(struct thread *thread);

