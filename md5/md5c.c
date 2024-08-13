#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dds/dds.h"
#include "md5_type.h"

static const char *saved_argv0;

static const MD5 data[] = {
  {{
    0xd1,0x31,0xdd,0x02,0xc5,0xe6,0xee,0xc4,0x69,0x3d,0x9a,0x06,0x98,0xaf,0xf9,0x5c,
    0x2f,0xca,0xb5,0x87,0x12,0x46,0x7e,0xab,0x40,0x04,0x58,0x3e,0xb8,0xfb,0x7f,0x89,
    0x55,0xad,0x34,0x06,0x09,0xf4,0xb3,0x02,0x83,0xe4,0x88,0x83,0x25,0x71,0x41,0x5a,
    0x08,0x51,0x25,0xe8,0xf7,0xcd,0xc9,0x9f,0xd9,0x1d,0xbd,0xf2,0x80,0x37,0x3c,0x5b,
    0xd8,0x82,0x3e,0x31,0x56,0x34,0x8f,0x5b,0xae,0x6d,0xac,0xd4,0x36,0xc9,0x19,0xc6,
    0xdd,0x53,0xe2,0xb4,0x87,0xda,0x03,0xfd,0x02,0x39,0x63,0x06,0xd2,0x48,0xcd,0xa0,
    0xe9,0x9f,0x33,0x42,0x0f,0x57,0x7e,0xe8,0xce,0x54,0xb6,0x70,0x80,0xa8,0x0d,0x1e,
    0xc6,0x98,0x21,0xbc,0xb6,0xa8,0x83,0x93,0x96,0xf9,0x65,0x2b,0x6f,0xf7,0x2a,0x70 },
    0, 0
  },
  {{
    0xd1,0x31,0xdd,0x02,0xc5,0xe6,0xee,0xc4,0x69,0x3d,0x9a,0x06,0x98,0xaf,0xf9,0x5c,
    0x2f,0xca,0xb5,0x07,0x12,0x46,0x7e,0xab,0x40,0x04,0x58,0x3e,0xb8,0xfb,0x7f,0x89,
    0x55,0xad,0x34,0x06,0x09,0xf4,0xb3,0x02,0x83,0xe4,0x88,0x83,0x25,0xf1,0x41,0x5a,
    0x08,0x51,0x25,0xe8,0xf7,0xcd,0xc9,0x9f,0xd9,0x1d,0xbd,0x72,0x80,0x37,0x3c,0x5b,
    0xd8,0x82,0x3e,0x31,0x56,0x34,0x8f,0x5b,0xae,0x6d,0xac,0xd4,0x36,0xc9,0x19,0xc6,
    0xdd,0x53,0xe2,0x34,0x87,0xda,0x03,0xfd,0x02,0x39,0x63,0x06,0xd2,0x48,0xcd,0xa0,
    0xe9,0x9f,0x33,0x42,0x0f,0x57,0x7e,0xe8,0xce,0x54,0xb6,0x70,0x80,0x28,0x0d,0x1e,
    0xc6,0x98,0x21,0xbc,0xb6,0xa8,0x83,0x93,0x96,0xf9,0x65,0xab,0x6f,0xf7,0x2a,0x70 },
    0, 0
  }
};

static size_t delta[sizeof (data[0].k)];
static size_t ndelta;
static void make_delta (void)
{
  ndelta = 0;
  for (size_t i = 0; i < sizeof (data[0].k); i++)
    if (data[0].k[i] != data[1].k[i])
      delta[ndelta++] = i;
}

static void usage (void)
{
  printf ("usage: %s sub | pub {A|B} {0|1} {0|1|write|dispose|unregister}...\n", saved_argv0);
  exit (1);
}

static const char *istr (dds_instance_state_t s)
{
  if (s == DDS_ALIVE_INSTANCE_STATE)
    return "A";
  else if (s == DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE)
    return "D";
  else
    return "U";
}

static const char *vstr (dds_view_state_t s)
{
  if (s == DDS_NEW_VIEW_STATE)
    return "N";
  else
    return "O";
}

static const char *sstr (dds_sample_state_t s)
{
  if (s == DDS_READ_SAMPLE_STATE)
    return "S";
  else
    return "F";
}

typedef struct instkeystr {
  char x[2 * sizeof(data[0].k) + 4];
} instkeystr_t;

static char *instkeystr(instkeystr_t *buf, const MD5 *x)
{
  assert (sizeof (buf->x) >= 2*ndelta + 4);
  for (size_t i = 0; i < ndelta; i++)
    snprintf (buf->x + 2*i, sizeof (buf->x) - 2*i, "%02x", x->k[delta[i]]);
  snprintf (buf->x + 2*ndelta, sizeof (buf->x) - 2*ndelta, "...");
  return buf->x;
}

static void print (const dds_sample_info_t *si, const MD5 *x)
{
  instkeystr_t keystr;
  printf ("  is/vs/ss %s/%s/%s ph %"PRIu64" ih %"PRIu64" key %s", istr(si->instance_state), vstr(si->view_state), sstr(si->sample_state), si->publication_handle, si->instance_handle, instkeystr (&keystr, x));
  if (si->valid_data)
    printf (" pubid %c seq %"PRId32, x->pubid, x->seq);
  printf ("\n");
}

static void dosub(const dds_entity_t dp, const dds_entity_t tp)
{
  const dds_entity_t rd = dds_create_reader (dp, tp, NULL, NULL);
  const dds_entity_t rdcond = dds_create_readcondition (rd, DDS_NOT_READ_SAMPLE_STATE);
  const dds_entity_t ws = dds_create_waitset (dp);
  dds_waitset_attach (ws, rdcond, 0);
  printf ("Waiting for data ...\n");
  while (true)
  {
    dds_waitset_wait (ws, NULL, 0, DDS_INFINITY);
    printf ("%"PRId64" RHC now:\n", dds_time());
    // damn: still no *easy* way to read everything ... but we know there are at most 4 here
    // (2 instances, keep-last-1 but possibly a valid + an invalid sample)
    dds_sample_info_t si[10];
    void *xs[10] = { NULL };
    int n = dds_read (rd, xs, si, 10, 10);
    for (int i = 0; i < n; i++)
      print (&si[i], xs[i]);
    dds_return_loan (rd, xs, n);
  }
}

static void dopub(dds_entity_t dp, const dds_entity_t tp, const char **args)
{
  if (!((strcmp (args[0], "A") == 0 || strcmp (args[0], "B") == 0) &&
        (strcmp (args[1], "0") == 0 || strcmp (args[1], "1") == 0)))
    usage ();
  const char pubid = (*args++)[0];
  int seq = 0;
  size_t inst = (size_t) atoi (*args++);

  dds_entity_t wr = dds_create_writer (dp, tp, NULL, NULL);

  dds_sleepfor (DDS_SECS (1));
  const char *cmd;
  while ((cmd = *args++) != NULL)
  {
    instkeystr_t keystr;
    printf ("%"PRId64"ns pub %c seq %d inst %s: %s\n", dds_time(), pubid, seq, instkeystr(&keystr, &data[inst]), cmd);
    if (strcmp (cmd, "0") == 0) {
      inst = 0;
    } else if (strcmp (cmd, "1") == 0) {
      inst = 1;
    } else if (strcmp (cmd, "write") == 0) {
      MD5 tmp = data[inst]; tmp.pubid = pubid; tmp.seq = seq++;
      dds_write(wr, &tmp);
    } else if (strcmp (cmd, "dispose") == 0) {
      MD5 tmp = data[inst]; tmp.pubid = pubid; tmp.seq = seq++;
      dds_dispose(wr, &tmp);
    } else if (strcmp (cmd, "unregister") == 0) {
      MD5 tmp = data[inst]; tmp.pubid = pubid; tmp.seq = seq++;
      dds_unregister_instance (wr, &tmp);
    } else {
      printf ("invalid operation\n");
      exit (1);
    }
    dds_sleepfor (DDS_SECS (2));
  }
  dds_sleepfor (DDS_SECS (5));
  printf ("%"PRId64" pub done\n", dds_time());
}

int main (int argc, const char **argv)
{
  make_delta ();
  saved_argv0 = argv[0];
  if (argc == 1)
    usage ();
  argv++;

  const dds_entity_t dp = dds_create_participant (0, NULL, NULL);
  dds_qos_t *tpqos = dds_create_qos ();
  dds_qset_reliability (tpqos, DDS_RELIABILITY_RELIABLE, DDS_INFINITY);
  dds_qset_durability (tpqos, DDS_DURABILITY_TRANSIENT_LOCAL);
  const dds_entity_t tp = dds_create_topic (dp, &MD5_desc, "MD5", tpqos, NULL);
  dds_delete_qos (tpqos);

  if (strcmp (argv[0], "sub") == 0)
    dosub(dp, tp);
  else
    dopub(dp, tp, ++argv);

  dds_delete (dp);
  return 0;
}
