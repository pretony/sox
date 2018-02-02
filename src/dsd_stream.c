#include "sox_i.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

struct dsd {
	uint32_t chan_num;
	uint32_t sfreq;
	uint32_t bps;
	uint64_t scount;
	uint32_t block_size;

	uint32_t block_pos;
	uint32_t bit_pos;
	uint8_t *block;
	uint64_t read_samp;
};

#define STREAM_SIZE 4096

static int dsd_startread(sox_format_t *ft)
{
	struct dsd *dsd = ft->priv;

	dsd->sfreq = ft->signal.rate;
	dsd->chan_num = ft->signal.channels;
	ft->signal.precision = 1;

    dsd->block_size = STREAM_SIZE;
    dsd->block_pos = dsd->block_size * 2;

    dsd->block = lsx_calloc(dsd->chan_num, (size_t)dsd->block_size);
    if (!dsd->block)
        return SOX_ENOMEM;


	ft->encoding.encoding = SOX_ENCODING_DSD;
	ft->encoding.bits_per_sample = 1;

	return SOX_SUCCESS;
}

static void dsd_read_bits(struct dsd *dsd, sox_sample_t *buf, unsigned len, DSDTYPE_t dsdtype)
{
    uint8_t *dsd_char = dsd->block + dsd->block_pos;
    unsigned i, j;

    for (i = 0; i < dsd->chan_num; i++) {
        unsigned d = dsd_char[i];

        for (j = 0; j < len; j++) {
            if(dsdtype == DSDTYPE_DSF) {
                buf[i + j * dsd->chan_num] = d & 1 ?
                        SOX_SAMPLE_MAX : -SOX_SAMPLE_MAX;
                d >>= 1;
            }
            else if(dsdtype == DSDTYPE_DFF) {
                buf[i + j * dsd->chan_num] = d & 128 ?
                        SOX_SAMPLE_MAX : -SOX_SAMPLE_MAX;
                d <<= 1;
            }
        }
    }
}

static size_t dsd_read(sox_format_t *ft, sox_sample_t *buf, size_t len)
{
    struct dsd *dsd = ft->priv;
    size_t rsamp = 0;

    len /= dsd->chan_num;

    while (len >= 8) {
        if (dsd->block_pos >= dsd->block_size * 2) {
            size_t rlen = dsd->chan_num * dsd->block_size;
            size_t rc = lsx_read_b_buf(ft, dsd->block, rlen);
            if (rc < rlen) {
                return rsamp * dsd->chan_num;
            }
            dsd->block_pos = 0;
        }

        dsd_read_bits(dsd, buf, 8, ft->dsdtype);
        buf += 8 * dsd->chan_num;
        dsd->block_pos+=2;
        rsamp += 8;
        len -= 8;
    }

    return rsamp * dsd->chan_num;
}

static int dsd_stopread(sox_format_t *ft)
{
	struct dsd *dsd = ft->priv;

	free(dsd->block);

	return SOX_SUCCESS;
}


LSX_FORMAT_HANDLER(dsd_stream)
{
	static char const * const names[] = { "dsd_stream", NULL };
	static unsigned const write_encodings[] = {
		SOX_ENCODING_DSD, 1, 0,
		0 };
	static sox_format_handler_t const handler = {
		SOX_LIB_VERSION_CODE,
		"Container for DSD data",
		names, SOX_FILE_LIT_END,
		dsd_startread, dsd_read, dsd_stopread,
		NULL, NULL, NULL,
		NULL, write_encodings, NULL,
		sizeof(struct dsd)
	};
	return &handler;
}
