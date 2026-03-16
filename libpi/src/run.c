// Adapted from: https://github.com/karpathy/llama2.c/blob/master/run.c
/* Inference for Llama-2 Transformer model in pure C */
#include "rpi.h"
#include "asm-helpers.h"
#include "uart.h"
#include "pt-vm.h"
// #include "armv6-pmu.h"
#include "pi-sd.h"
#include "fat32.h"
#include "mbr.h"
#include "multicore.h"
#include <arm_neon.h>

#include "libc/parse-byte-token.c"
#include "libc/isspace.c"
#include "libc/print_float.c"
#include "libc/expf.h"
#include "libc/powf.h"
#include "libc/sqrtf.c"
#include "libc/cosf.c"
#include "libc/sinf.c"
#include "libc/bsearch.c"
#include "libc/qsort.c"
#include "libc/isprint.c"

#include "gprof.c"

#define SEG_HEAP     (1024 * 1024)
#define HEAP_SIZE_MB FAT32_HEAP_MB
typedef struct {
    int dim; // transformer dimension
    int hidden_dim; // for ffn layers
    int n_layers; // number of layers
    int n_heads; // number of query heads
    int n_kv_heads; // number of key/value heads (can be < query heads because of multiquery)
    int vocab_size; // vocabulary size, usually 256 (byte-level)
    int seq_len; // max sequence length
} Config;

typedef struct {
    // token embedding table
    float* token_embedding_table;    // (vocab_size, dim)
    // weights for rmsnorms
    float* rms_att_weight; // (layer, dim) rmsnorm weights
    float* rms_ffn_weight; // (layer, dim)
    // weights for matmuls. note dim == n_heads * head_size
    float* wq; // (layer, dim, n_heads * head_size)
    float* wk; // (layer, dim, n_kv_heads * head_size)
    float* wv; // (layer, dim, n_kv_heads * head_size)
    float* wo; // (layer, n_heads * head_size, dim)
    // weights for ffn
    float* w1; // (layer, hidden_dim, dim)
    float* w2; // (layer, dim, hidden_dim)
    float* w3; // (layer, hidden_dim, dim)
    // final rmsnorm
    float* rms_final_weight; // (dim,)
    // (optional) classifier weights for the logits, on the last layer
    float* wcls;
} TransformerWeights;

typedef struct {
    // current wave of activations
    float *x; // activation at current time stamp (dim,)
    float *xb; // same, but inside a residual branch (dim,)
    float *xb2; // an additional buffer just for convenience (dim,)
    float *hb; // buffer for hidden dimension in the ffn (hidden_dim,)
    float *hb2; // buffer for hidden dimension in the ffn (hidden_dim,)
    float *q; // query (dim,)
    float *k; // key (dim,)
    float *v; // value (dim,)
    float *att; // buffer for scores/attention values (n_heads, seq_len)
    float *logits; // output logits
    // kv cache
    float* key_cache;   // (layer, seq_len, dim)
    float* value_cache; // (layer, seq_len, dim)
} RunState;

typedef struct {
    Config config; // the hyperparameters of the architecture (the blueprint)
    TransformerWeights weights; // the weights of the model
    RunState state; // buffers for the "wave" of activations in the forward pass
    // some more state needed to properly clean up the memory mapping (sigh)
    int fd; // file descriptor for memory mapping
    float* data; // memory mapped data pointer
    long file_size; // size of the checkpoint file in bytes
} Transformer;

mbr_t *mbr;
mbr_partition_ent_t partition_run;
fat32_fs_t fs;
pi_dirent_t root;

// TEMPORARY FREE IMPLEMENTATION THAT IS JUST A NO-OP
void free(void *ptr) {
    return;
}

void *calloc(size_t nelem, size_t elsize) {
    return kmalloc(nelem * elsize);
}

void malloc_run_state(RunState* s, Config* p) {
    // we calloc instead of malloc to keep valgrind happy
    int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;
    s->x = calloc(p->dim, sizeof(float));
    s->xb = calloc(p->dim, sizeof(float));
    s->xb2 = calloc(p->dim, sizeof(float));
    s->hb = calloc(p->hidden_dim, sizeof(float));
    s->hb2 = calloc(p->hidden_dim, sizeof(float));
    s->q = calloc(p->dim, sizeof(float));
    s->key_cache = calloc(p->n_layers * p->seq_len * kv_dim, sizeof(float));
    s->value_cache = calloc(p->n_layers * p->seq_len * kv_dim, sizeof(float));
    s->att = calloc(p->n_heads * p->seq_len, sizeof(float));
    s->logits = calloc(p->vocab_size, sizeof(float));
    // ensure all mallocs went fine
    if (!s->x || !s->xb || !s->xb2 || !s->hb || !s->hb2 || !s->q
     || !s->key_cache || !s->value_cache || !s->att || !s->logits) {
        panic("malloc failed!\n");
    }
}

void memory_map_weights(TransformerWeights *w, Config* p, float* ptr, int shared_weights) {
    int head_size = p->dim / p->n_heads;
    // make sure the multiplications below are done in 64bit to fit the parameter counts of 13B+ models
    unsigned long long n_layers = p->n_layers;
    w->token_embedding_table = ptr;
    ptr += p->vocab_size * p->dim;
    w->rms_att_weight = ptr;
    ptr += n_layers * p->dim;
    w->wq = ptr;
    ptr += n_layers * p->dim * (p->n_heads * head_size);
    w->wk = ptr;
    ptr += n_layers * p->dim * (p->n_kv_heads * head_size);
    w->wv = ptr;
    ptr += n_layers * p->dim * (p->n_kv_heads * head_size);
    w->wo = ptr;
    ptr += n_layers * (p->n_heads * head_size) * p->dim;
    w->rms_ffn_weight = ptr;
    ptr += n_layers * p->dim;
    w->w1 = ptr;
    ptr += n_layers * p->dim * p->hidden_dim;
    w->w2 = ptr;
    ptr += n_layers * p->hidden_dim * p->dim;
    w->w3 = ptr;
    ptr += n_layers * p->dim * p->hidden_dim;
    w->rms_final_weight = ptr;
    ptr += p->dim;
    ptr += p->seq_len * head_size / 2; // skip what used to be freq_cis_real (for RoPE)
    ptr += p->seq_len * head_size / 2; // skip what used to be freq_cis_imag (for RoPE)
    w->wcls = shared_weights ? w->token_embedding_table : ptr;
}

void read_checkpoint(char* checkpoint, Config* config, TransformerWeights* weights,
                     int* fd, float** data, long* file_size) {
    printk("Reading checkpoint: %s\n", checkpoint);

    pi_dirent_t root_copy = root;
    pi_file_t *file = fat32_read(&fs, &root_copy, checkpoint);
    if (!file || !file->data) {
        // handle if file is empty or doesn't exist
        panic("Couldn't open file %s\n", checkpoint);
    }
    if (file->n_data < sizeof(Config)) {
        panic("Checkpoint file too small");
    }
    char *src = file->data;
    char *dest = (char*)config;
    for (unsigned long i = 0; i < sizeof(Config); i++) {
        dest[i] = src[i];
    }
    int shared_weights = config->vocab_size > 0 ? 1 : 0;
    config->vocab_size = config->vocab_size < 0 ? -(config->vocab_size) : config->vocab_size;
    *file_size = file->n_data;
    *data = (float*)file->data;
    float *weights_ptr = *data + sizeof(Config)/sizeof(float);
    memory_map_weights(weights, config, weights_ptr, shared_weights);
}

void build_transformer(Transformer *t, char* checkpoint_path) {
    // read in the Config and the Weights from the checkpoint
    read_checkpoint(checkpoint_path, &t->config, &t->weights, &t->fd, &t->data, &t->file_size);
    // allocate the RunState buffers
    malloc_run_state(&t->state, &t->config);
}

void rmsnorm(float* o, float* x, float* weight, int size) {
    // calculate sum of squares
    float ss = 0.0f;
    for (int j = 0; j < size; j++) {
        ss += x[j] * x[j];
    }
    ss /= size;
    ss += 1e-5f;
    ss = 1.0f / sqrtf(ss);
    // normalize and scale
    for (int j = 0; j < size; j++) {
        o[j] = weight[j] * (ss * x[j]);
    }
}

void softmax(float* x, int size) {
    // find max value (for numerical stability)
    float max_val = x[0];
    for (int i = 1; i < size; i++) {
        if (x[i] > max_val) {
            max_val = x[i];
        }
    }
    // exp and sum
    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        x[i] = expf(x[i] - max_val);
        sum += x[i];
    }
    // normalize
    for (int i = 0; i < size; i++) {
        x[i] /= sum;
    }
}

void matmul(float *xout, float *x, float *w, int n, int d) {
    for (int i = 0; i < d; i++) {
        float *row = w + i * n;
        float32x4_t acc = vdupq_n_f32(0.0f);
        int j = 0;
        for (; j <= n - 4; j += 4)
            acc = vmlaq_f32(acc, vld1q_f32(row + j), vld1q_f32(x + j));
        float32x2_t s = vadd_f32(vget_high_f32(acc), vget_low_f32(acc));
        s = vpadd_f32(s, s);
        float val = vget_lane_f32(s, 0);
        for (; j < n; j++) val += row[j] * x[j];
        xout[i] = val;
    }
}

float* forward(Transformer* transformer, int token, int pos) {

    // a few convenience variables
    Config* p = &transformer->config;
    TransformerWeights* w = &transformer->weights;
    RunState* s = &transformer->state;
    float *x = s->x;
    int dim = p->dim;
    int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;
    int kv_mul = p->n_heads / p->n_kv_heads; // integer multiplier of the kv sharing in multiquery
    int hidden_dim =  p->hidden_dim;
    int head_size = dim / p->n_heads;
    float head_size_inv_sqrt = 1.0f / sqrtf(head_size);

    // copy the token embedding into x
    float* content_row = w->token_embedding_table + token * dim;
    memcpy(x, content_row, dim*sizeof(*x));

    // forward all the layers
    for(unsigned long long l = 0; l < p->n_layers; l++) {

        // attention rmsnorm
        rmsnorm(s->xb, x, w->rms_att_weight + l*dim, dim);

        // key and value point to the kv cache
        int loff = l * p->seq_len * kv_dim; // kv cache layer offset for convenience
        s->k = s->key_cache + loff + pos * kv_dim;
        s->v = s->value_cache + loff + pos * kv_dim;

        // qkv matmuls for this position
        matmul(s->q, s->xb, w->wq + l*dim*dim, dim, dim);
        matmul(s->k, s->xb, w->wk + l*dim*kv_dim, dim, kv_dim);
        matmul(s->v, s->xb, w->wv + l*dim*kv_dim, dim, kv_dim);

        // RoPE relative positional encoding: complex-valued rotate q and k in each head
        for (int i = 0; i < dim; i+=2) {
            int head_dim = i % head_size;
            float freq = 1.0f / powf(10000.0f, head_dim / (float)head_size);
            float val = pos * freq;
            float fcr = cosf(val);
            float fci = sinf(val);
            int rotn = i < kv_dim ? 2 : 1; // how many vectors? 2 = q & k, 1 = q only
            for (int v = 0; v < rotn; v++) {
                float* vec = v == 0 ? s->q : s->k; // the vector to rotate (query or key)
                float v0 = vec[i];
                float v1 = vec[i+1];
                vec[i]   = v0 * fcr - v1 * fci;
                vec[i+1] = v0 * fci + v1 * fcr;
            }
        }

        // multihead attention. iterate over all heads
        int h;
        for (h = 0; h < p->n_heads; h++) {
            // get the query vector for this head
            float* q = s->q + h * head_size;
            // attention scores for this head
            float* att = s->att + h * p->seq_len;
            // iterate over all timesteps, including the current one
            for (int t = 0; t <= pos; t++) {
                // get the key vector for this head and at this timestep
                float* k = s->key_cache + loff + t * kv_dim + (h / kv_mul) * head_size;
                // calculate the attention score as the dot product of q and k
                float score = 0.0f;
                for (int i = 0; i < head_size; i++) {
                    score += q[i] * k[i];
                }
                score *= head_size_inv_sqrt;
                // save the score to the attention buffer
                att[t] = score;
            }

            // softmax the scores to get attention weights, from 0..pos inclusively
            softmax(att, pos + 1);

            // weighted sum of the values, store back into xb
            float* xb = s->xb + h * head_size;
            memset(xb, 0, head_size * sizeof(float));
            for (int t = 0; t <= pos; t++) {
                // get the value vector for this head and at this timestep
                float* v = s->value_cache + loff + t * kv_dim + (h / kv_mul) * head_size;
                // get the attention weight for this timestep
                float a = att[t];
                // accumulate the weighted value into xb
                for (int i = 0; i < head_size; i++) {
                    xb[i] += a * v[i];
                }
            }
        }

        // final matmul to get the output of the attention
        matmul(s->xb2, s->xb, w->wo + l*dim*dim, dim, dim);

        // residual connection back into x
        for (int i = 0; i < dim; i++) {
            x[i] += s->xb2[i];
        }

        // ffn rmsnorm
        rmsnorm(s->xb, x, w->rms_ffn_weight + l*dim, dim);

        // Now for FFN in PyTorch we have: self.w2(F.silu(self.w1(x)) * self.w3(x))
        // first calculate self.w1(x) and self.w3(x)
        matmul(s->hb, s->xb, w->w1 + l*dim*hidden_dim, dim, hidden_dim);
        matmul(s->hb2, s->xb, w->w3 + l*dim*hidden_dim, dim, hidden_dim);

        // SwiGLU non-linearity
        for (int i = 0; i < hidden_dim; i++) {
            float val = s->hb[i];
            // silu(x)=x*σ(x), where σ(x) is the logistic sigmoid
            val *= (1.0f / (1.0f + expf(-val)));
            // elementwise multiply with w3(x)
            val *= s->hb2[i];
            s->hb[i] = val;
        }

        // final matmul to get the output of the ffn
        matmul(s->xb, s->hb, w->w2 + l*dim*hidden_dim, hidden_dim, dim);

        // residual connection
        for (int i = 0; i < dim; i++) {
            x[i] += s->xb[i];
        }
    }

    // final rmsnorm
    rmsnorm(x, x, w->rms_final_weight, dim);

    // classifier into logits
    matmul(s->logits, x, w->wcls, p->dim, p->vocab_size);
    return s->logits;
}

// ----------------------------------------------------------------------------
// The Byte Pair Encoding (BPE) Tokenizer that translates strings <-> tokens

typedef struct {
    char *str;
    int id;
} TokenIndex;

typedef struct {
    char** vocab;
    float* vocab_scores;
    TokenIndex *sorted_vocab;
    int vocab_size;
    unsigned int max_token_length;
    unsigned char byte_pieces[512]; // stores all single-byte strings
} Tokenizer;

int compare_tokens(const void *a, const void *b) {
    return strcmp(((TokenIndex*)a)->str, ((TokenIndex*)b)->str);
}

void build_tokenizer(Tokenizer* t, char* tokenizer_path, int vocab_size) {
    // i should have written the vocab_size into the tokenizer file... sigh
    t->vocab_size = vocab_size;
    // malloc space to hold the scores and the strings
    t->vocab = (char**)kmalloc(vocab_size * sizeof(char*));
    t->vocab_scores = (float*)kmalloc(vocab_size * sizeof(float));
    t->sorted_vocab = NULL; // initialized lazily
    for (int i = 0; i < 256; i++) {
        t->byte_pieces[i * 2] = (unsigned char)i;
        t->byte_pieces[i * 2 + 1] = '\0';
    }
    // read in the file
    pi_dirent_t root_copy = root;
    pi_file_t *file = fat32_read(&fs, &root_copy, tokenizer_path);
    if (!file || !file->data) {
        panic("couldn't load %s\n", tokenizer_path);
    }
    char *cursor = file->data;
    if (file->n_data < sizeof(int)) {
        panic("failed read\n");
    }
    memcpy(&t->max_token_length, cursor, sizeof(int));
    cursor += sizeof(int);

    // Read each vocabulary entry
    for (int i = 0; i < vocab_size; i++) {
        // Check if we have enough data left to read a float
        if (cursor + sizeof(float) > file->data + file->n_data) {
            panic("failed read\n"); 
        }
        
        // Read the score (a float)
        memcpy(&t->vocab_scores[i], cursor, sizeof(float));
        cursor += sizeof(float);
        
        // Read the length of the token string (an int)
        if (cursor + sizeof(int) > file->data + file->n_data) {
            panic("failed read\n"); 
        }
        int len;
        memcpy(&len, cursor, sizeof(int));
        cursor += sizeof(int);
        
        // Allocate memory for the token string and read it
        t->vocab[i] = (char *)kmalloc(len + 1);
        if (cursor + len > file->data + file->n_data) {
            panic("failed read\n"); 
        }
        
        // Copy the string data
        for (int j = 0; j < len; j++) {
            t->vocab[i][j] = cursor[j];
        }
        cursor += len;
        
        // Null-terminate the string
        t->vocab[i][len] = '\0';
    }
}

char* decode(Tokenizer* t, int prev_token, int token) {
    char *piece = t->vocab[token];
    // following BOS (1) token, sentencepiece decoder strips any leading whitespace (see PR #89)
    if (prev_token == 1 && piece[0] == ' ') { piece++; }
    // careful, some tokens designate raw bytes, and look like e.g. '<0x01>'
    // parse this and convert and return the actual byte
    unsigned char byte_val;
    if (parse_byte_token(piece, &byte_val)) {
        piece = (char*)t->byte_pieces + byte_val * 2;
    }
    return piece;
}

void safe_printf(char *piece) {
    // piece might be a raw byte token, and we only want to print printable chars or whitespace
    // because some of the other bytes can be various control codes, backspace, etc.
    if (piece == NULL) { return; }
    if (piece[0] == '\0') { return; }
    if (piece[1] == '\0') {
        unsigned char byte_val = piece[0];
        if (!(isprint(byte_val) || isspace(byte_val))) {
            return; // bad byte, don't print it
        }
    }
    printk("%s", piece);
}

int str_lookup(char *str, TokenIndex *sorted_vocab, int vocab_size) {
    // efficiently find the perfect match for str in vocab, return its index or -1 if not found
    TokenIndex tok = { .str = str }; // acts as the key to search for
    TokenIndex *res = bsearch(&tok, sorted_vocab, vocab_size, sizeof(TokenIndex), compare_tokens);
    return res != NULL ? res->id : -1;
}

void encode(Tokenizer* t, char *text, int8_t bos, int8_t eos, int *tokens, int *n_tokens) {
    // encode the string text (input) into an upper-bound preallocated tokens[] array
    // bos != 0 means prepend the BOS token (=1), eos != 0 means append the EOS token (=2)
    if (text == NULL) { panic("cannot encode NULL text\n"); }

    if (t->sorted_vocab == NULL) {
        // lazily malloc and sort the vocabulary
        t->sorted_vocab = kmalloc(t->vocab_size * sizeof(TokenIndex));
        for (int i = 0; i < t->vocab_size; i++) {
            t->sorted_vocab[i].str = t->vocab[i];
            t->sorted_vocab[i].id = i;
        }
        qsort(t->sorted_vocab, t->vocab_size, sizeof(TokenIndex), compare_tokens);
    }

    // create a temporary buffer that will store merge candidates of always two consecutive tokens
    // *2 for concat, +1 for null terminator +2 for UTF8 (in case max_token_length is 1)
    size_t str_buffer_size = (t->max_token_length*2 +1 +2) * sizeof(char);
    char* str_buffer = kmalloc(str_buffer_size);
    size_t str_len = 0;

    // start at 0 tokens
    *n_tokens = 0;

    // add optional BOS (=1) token, if desired
    if (bos) tokens[(*n_tokens)++] = 1;

    // add_dummy_prefix is true by default
    // so prepend a dummy prefix token to the input string, but only if text != ""
    // TODO: pretty sure this isn't correct in the general case but I don't have the
    // energy to read more of the sentencepiece code to figure out what it's doing
    if (text[0] != '\0') {
        int dummy_prefix = str_lookup(" ", t->sorted_vocab, t->vocab_size);
        tokens[(*n_tokens)++] = dummy_prefix;
    }

    // Okay UTF-8 time. This will get messy. Here is the reference from Wikipedia:
    // Code point ↔ UTF-8 conversion
    // First code point	Last code point	Byte 1	Byte 2	Byte 3	Byte 4
    // U+0000	U+007F	    0xxxxxxx
    // U+0080	U+07FF	    110xxxxx	10xxxxxx
    // U+0800	U+FFFF	    1110xxxx	10xxxxxx	10xxxxxx
    // U+10000	U+10FFFF    11110xxx	10xxxxxx	10xxxxxx	10xxxxxx

    // process the raw (UTF-8) byte sequence of the input string
    for (char *c = text; *c != '\0'; c++) {

        // reset buffer if the current byte is ASCII or a leading byte
        // 0xC0 is 11000000, so (*c & 0xC0) keeps the first 2 bits and zeros the rest
        // 0x80 is 10000000
        // in UTF-8, all continuation bytes start with "10" in first two bits
        // so in English this is: "if this byte is not a continuation byte"
        if ((*c & 0xC0) != 0x80) {
            // this byte must be either a leading byte (11...) or an ASCII char (0x...)
            // => reset our location, as we're starting a new UTF-8 codepoint
            str_len = 0;
        }

        // append the current byte to the buffer
        str_buffer[str_len++] = *c; // ++ is post-increment, incremented after this line
        str_buffer[str_len] = '\0';

        // while the next character is a continuation byte, continue appending
        // but if there are too many of them, just stop to avoid overruning str_buffer size.
        if ((*(c+1) & 0xC0) == 0x80 && str_len < 4) {
            continue;
        }

        // ok c+1 is not a continuation byte, so we've read in a full codepoint
        int id = str_lookup(str_buffer, t->sorted_vocab, t->vocab_size);

        if (id != -1) {
            // we found this codepoint in vocab, add it as a token
            tokens[(*n_tokens)++] = id;
        } else {
            // byte_fallback encoding: just encode each byte as a token
            // +3 is here because the first 3 vocab elements are <unk>, <s>, </s>
            // so the individual bytes only start at index 3
            for (int i=0; i < str_len; i++) {
                tokens[(*n_tokens)++] = (unsigned char)str_buffer[i] + 3;
            }
        }
        str_len = 0; // protect against a sequence of stray UTF8 continuation bytes
    }

    // merge the best consecutive pair each iteration, according the scores in vocab_scores
    while (1) {
        float best_score = -1e10;
        int best_id = -1;
        int best_idx = -1;

        for (int i=0; i < (*n_tokens-1); i++) {
            // check if we can merge the pair (tokens[i], tokens[i+1])
            // sprintf(str_buffer, "%s%s", t->vocab[tokens[i]], t->vocab[tokens[i+1]]);
            //printk(str_buffer, str_buffer_size, "%s%s", t->vocab[tokens[i]], t->vocab[tokens[i+1]]);
            int id = str_lookup(str_buffer, t->sorted_vocab, t->vocab_size);
            if (id != -1 && t->vocab_scores[id] > best_score) {
                // this merge pair exists in vocab! record its score and position
                best_score = t->vocab_scores[id];
                best_id = id;
                best_idx = i;
            }
        }

        if (best_idx == -1) {
            break; // we couldn't find any more pairs to merge, so we're done
        }

        // merge the consecutive pair (best_idx, best_idx+1) into new token best_id
        tokens[best_idx] = best_id;
        // delete token at position best_idx+1, shift the entire sequence back 1
        for (int i = best_idx+1; i < (*n_tokens-1); i++) {
            tokens[i] = tokens[i+1];
        }
        (*n_tokens)--; // token length decreased
    }

    // add optional EOS (=2) token, if desired
    if (eos) tokens[(*n_tokens)++] = 2;

    free(str_buffer);
}

// ----------------------------------------------------------------------------
// The Sampler, which takes logits and returns a sampled token
// sampling can be done in a few ways: greedy argmax, sampling, top-p sampling

typedef struct {
    float prob;
    int index;
} ProbIndex; // struct used when sorting probabilities during top-p sampling

typedef struct {
    int vocab_size;
    ProbIndex* probindex; // buffer used in top-p sampling
    float temperature;
    float topp;
    unsigned long long rng_state;
} Sampler;

int sample_argmax(float* probabilities, int n) {
    // return the index that has the highest probability
    int max_i = 0;
    float max_p = probabilities[0];
    for (int i = 1; i < n; i++) {
        if (probabilities[i] > max_p) {
            max_i = i;
            max_p = probabilities[i];
        }
    }
    return max_i;
}

int sample_mult(float* probabilities, int n, float coin) {
    // sample index from probabilities (they must sum to 1!)
    // coin is a random number in [0, 1), usually from random_f32()
    float cdf = 0.0f;
    for (int i = 0; i < n; i++) {
        cdf += probabilities[i];
        if (coin < cdf) {
            return i;
        }
    }
    return n - 1; // in case of rounding errors
}

int compare(const void* a, const void* b) {
    ProbIndex* a_ = (ProbIndex*) a;
    ProbIndex* b_ = (ProbIndex*) b;
    if (a_->prob > b_->prob) return -1;
    if (a_->prob < b_->prob) return 1;
    return 0;
}

int sample_topp(float* probabilities, int n, float topp, ProbIndex* probindex, float coin) {
    // top-p sampling (or "nucleus sampling") samples from the smallest set of
    // tokens that exceed probability topp. This way we never sample tokens that
    // have very low probabilities and are less likely to go "off the rails".
    // coin is a random number in [0, 1), usually from random_f32()

    int n0 = 0;
    // quicksort indices in descending order of probabilities
    // values smaller than (1 - topp) / (n - 1) cannot be part of the result
    // so for efficiency we crop these out as candidates before sorting
    const float cutoff = (1.0f - topp) / (n - 1);
    for (int i = 0; i < n; i++) {
        if (probabilities[i] >= cutoff) {
            probindex[n0].index = i;
            probindex[n0].prob = probabilities[i];
            n0++;
        }
    }
    qsort(probindex, n0, sizeof(ProbIndex), compare);

    // truncate the list where cumulative probability exceeds topp
    float cumulative_prob = 0.0f;
    int last_idx = n0 - 1; // in case of rounding errors consider all elements
    for (int i = 0; i < n0; i++) {
        cumulative_prob += probindex[i].prob;
        if (cumulative_prob > topp) {
            last_idx = i;
            break; // we've exceeded topp by including last_idx
        }
    }

    // sample from the truncated list
    float r = coin * cumulative_prob;
    float cdf = 0.0f;
    for (int i = 0; i <= last_idx; i++) {
        cdf += probindex[i].prob;
        if (r < cdf) {
            return probindex[i].index;
        }
    }
    return probindex[last_idx].index; // in case of rounding errors
}

void build_sampler(Sampler* sampler, int vocab_size, float temperature, float topp, unsigned long long rng_seed) {
    sampler->vocab_size = vocab_size;
    sampler->temperature = temperature;
    sampler->topp = topp;
    sampler->rng_state = rng_seed;
    // buffer only used with nucleus sampling; may not need but it's ~small
    sampler->probindex = kmalloc(sampler->vocab_size * sizeof(ProbIndex));
}

unsigned int random_u32(unsigned long long *state) {
    // xorshift rng: https://en.wikipedia.org/wiki/Xorshift#xorshift.2A
    *state ^= *state >> 12;
    *state ^= *state << 25;
    *state ^= *state >> 27;
    return (*state * 0x2545F4914F6CDD1Dull) >> 32;
}
float random_f32(unsigned long long *state) { // random float32 in [0,1)
    return (random_u32(state) >> 8) / 16777216.0f;
}

int sample(Sampler* sampler, float* logits) {
    // sample the token given the logits and some hyperparameters
    int next;
    if (sampler->temperature == 0.0f) {
        // greedy argmax sampling: take the token with the highest probability
        next = sample_argmax(logits, sampler->vocab_size);
    } else {
        // apply the temperature to the logits
        for (int q=0; q<sampler->vocab_size; q++) { logits[q] /= sampler->temperature; }
        // apply softmax to the logits to get the probabilities for next token
        softmax(logits, sampler->vocab_size);
        // flip a (float) coin (this is our source of entropy for sampling)
        float coin = random_f32(&sampler->rng_state);
        // we sample from this distribution to get the next token
        if (sampler->topp <= 0 || sampler->topp >= 1) {
            // simply sample from the predicted probability distribution
            next = sample_mult(logits, sampler->vocab_size, coin);
        } else {
            // top-p (nucleus) sampling, clamping the least likely tokens to zero
            next = sample_topp(logits, sampler->vocab_size, sampler->topp, sampler->probindex, coin);
        }
    }
    return next;
}

// ----------------------------------------------------------------------------
// utilities: time

long time_in_ms() {
    // return time in milliseconds, for benchmarking the model speed
    // struct timespec time;
    // clock_gettime(CLOCK_REALTIME, &time);
    // return time.tv_sec * 1000 + time.tv_nsec / 1000000;
    // Using timer_get_usec() which RETURNS 32-BIT SO IT WILL WRAP AROUND EVENTUALLY
    return (long) (timer_get_usec() / 1000);
}

// ----------------------------------------------------------------------------
// generation loop

void generate(Transformer *transformer, Tokenizer *tokenizer, Sampler *sampler, char *prompt, int steps) {
    char *empty_prompt = "";
    if (prompt == NULL) { prompt = empty_prompt; }

    // encode the (string) prompt into tokens sequence
    int num_prompt_tokens = 0;
    int* prompt_tokens = (int*)kmalloc((strlen(prompt)+3) * sizeof(int)); // +3 for '\0', ?BOS, ?EOS
    encode(tokenizer, prompt, 1, 0, prompt_tokens, &num_prompt_tokens);
    if (num_prompt_tokens < 1) {
        panic("something is wrong, expected at least 1 prompt token\n");
    }

    // start the main loop
    long start = 0;  // used to time our code, only initialized after first iteration
    int next;        // will store the next token in the sequence
    int token = prompt_tokens[0]; // kick off with the first token in the prompt
    int pos = 0;     // position in the sequence
    while (pos < steps) {

        // forward the transformer to get logits for the next token
        float* logits = forward(transformer, token, pos);

        // advance the state machine
        if (pos < num_prompt_tokens - 1) {
            // if we are still processing the input prompt, force the next prompt token
            next = prompt_tokens[pos + 1];
        } else {
            // otherwise sample the next token from the logits
            next = sample(sampler, logits);
        }
        pos++;

        // data-dependent terminating condition: the BOS (=1) token delimits sequences
        if (next == 1) { break; }

        // print the token as string, decode it with the Tokenizer object
        char* piece = decode(tokenizer, token, next);
        safe_printf(piece); // same as printf("%s", piece), but skips "unsafe" bytes
        // when using printf, you need flush to force stdout buffer to print out
        // but we are using //printk so comment out
        // fflush(stdout);
        token = next;

        // init the timer here because the first iteration can be slower
        if (start == 0) { start = time_in_ms(); }
    }
    printk("\n");

    // report achieved tok/s (pos-1 because the timer starts after first iteration)
    if (pos > 1) {
        long end = time_in_ms();
        printk("achieved tok/s: ");
        print_float((pos-1) / (double)(end-start)*1000);
        printk("\n");
        // //printk("achieved tok/s: %f\n", (pos-1) / (double)(end-start)*1000);
    }

    free(prompt_tokens);
}

// ----------------------------------------------------------------------------
// CLI, include only if not testing
#ifndef TESTING

// void error_usage() {
//     fprintf(stderr, "Usage:   run <checkpoint> [options]\n");
//     fprintf(stderr, "Example: run model.bin -n 256 -i \"Once upon a time\"\n");
//     fprintf(stderr, "Options:\n");
//     fprintf(stderr, "  -t <float>  temperature in [0,inf], default 1.0\n");
//     fprintf(stderr, "  -p <float>  p value in top-p (nucleus) sampling in [0,1] default 0.9\n");
//     fprintf(stderr, "  -s <int>    random seed, default time(NULL)\n");
//     fprintf(stderr, "  -n <int>    number of steps to run for, default 256. 0 = max_seq_len\n");
//     fprintf(stderr, "  -i <string> input prompt\n");
//     fprintf(stderr, "  -z <string> optional path to custom tokenizer\n");
//     fprintf(stderr, "  -m <string> mode: generate|chat, default: generate\n");
//     fprintf(stderr, "  -y <string> (optional) system prompt in chat mode\n");
//     exit(EXIT_FAILURE);
// }

static inline void enable_vfp(void) {
    // 1) Enable coprocessor access for CP10 and CP11
    unsigned r;
    asm volatile ("mrc p15, 0, %0, c1, c0, 2" : "=r"(r));
    r |= (0xF << 20); // full access for CP10 and CP11
    asm volatile ("mcr p15, 0, %0, c1, c0, 2" : : "r"(r));
    
    // Memory barrier
    dsb();
    prefetch_flush();

    // 2) Enable the VFP itself by setting FPEXC.EN
    asm volatile ("vmrs %0, fpexc" : "=r"(r));
    r |= 1 << 30;
    asm volatile ("vmsr fpexc, %0" : : "r"(r));

    // 3) Optionally flush subnormals to zero / default NaN
    //    This prevents subnormal stalls.
    asm volatile ("vmrs %0, fpscr" : "=r"(r));
    r |= 1 << 24;  // FZ (Flush-to-zero)
    r |= 1 << 25;  // DN (Default-NaN), optional
    asm volatile ("vmsr fpscr, %0" : : "r"(r));

    dsb();
    prefetch_flush();
}


// ------ controls here -------
// MUST ALSO CHANGE MAKEFILE FOR VFP!!!!!
#define VFP 1
#define CACHING 1
#define MULTICORE 1
#define TIMER_PROFILING 0
#define CHECKPOINT_PATH "MODEL.BIN"
#define TOKENIZER_PATH "T.BIN"
#define STEPS 10

#if CACHING
void flush_caches(void);

static void setup_vm(void) {
    enum { OneMB = 1024 * 1024 };
    enum { kern_dom = 1, kern_asid = 1, kern_pid = 0x140e };

    uint32_t dom_reg = DOM_client << (kern_dom * 2);
    vm_mmu_init(dom_reg);

    vm_pt_t *pt = vm_pt_alloc(4096);

    pin_t cached = pin_mk_global(kern_dom, perm_rw_priv, MEM_wb_alloc);
    pin_t device = pin_mk_device(kern_dom);

    // code: 1 section at 0x0
    vm_map_sec(pt, 0x0, 0x0, cached);

    // heap: identity-map each 1MB section
    uint32_t heap_start = (uint32_t)kmalloc_heap_start();
    uint32_t heap_end   = (uint32_t)kmalloc_heap_end();
    for (uint32_t addr = heap_start; addr < heap_end; addr += OneMB)
        vm_map_sec(pt, addr, addr, cached);

    // stacks (STACK_ADDR section may already be covered by heap; harmless)
    vm_map_sec(pt, STACK_ADDR - OneMB, STACK_ADDR - OneMB, cached);
    vm_map_sec(pt, INT_STACK_ADDR - OneMB, INT_STACK_ADDR - OneMB, cached);

    // BCM2837 peripherals: full 16MB range at 0x3F000000–0x3FFFFFFF
    // (EMMC/SD at 0x3F300000, USB at 0x3F980000, etc.)
    for (uint32_t addr = 0x3F000000; addr < 0x40000000; addr += OneMB)
        vm_map_sec(pt, addr, addr, device);

    // ARM local peripherals
    vm_map_sec(pt, 0x40000000, 0x40000000, device);

    vm_mmu_switch(pt, kern_pid, kern_asid);
    mmu_sync_pte_mods();
    vm_mmu_enable();

    // enable L1 D-cache, I-cache, branch predictor
    cp15_ctrl_reg1_t c = cp15_ctrl_reg1_rd();
    c.C_unified_enable = 1;
    c.I_icache_enable = 1;
    c.Z_branch_pred = 1;
    cp15_ctrl_reg1_wr(c);

    printk("setup_vm: MMU and caches enabled (heap=%dMB)\n",
           (heap_end - heap_start) / OneMB);
}
#endif

#if MULTICORE
static void worker_enable_vfp(void *arg) { (void)arg; enable_vfp(); }
#endif

void notmain_llama_inference(void) {
    uart_init();
    unsigned long MB = 1024*1024;
    kmalloc_init_set_start((void*)SEG_HEAP, HEAP_SIZE_MB*MB);

    #if MULTICORE
    multicore_init();
    printk("multicore: %d cores online\n", NUM_CORES);
    #endif

    #if CACHING
    setup_vm();
    printk("setup_vm: done\n");
    #endif
    pi_sd_init();

    printk("pi_sd_init: done\n");

    #if VFP
    enable_vfp();
    #if MULTICORE
    for (int c = 1; c < NUM_CORES; c++)
        multicore_call(c, worker_enable_vfp, NULL);
    printk("multicore: VFP enabled on all cores\n");
    #endif
    #endif

    mbr = mbr_read();
    memcpy(&partition_run, mbr->part_tab1, sizeof(mbr_partition_ent_t));
    assert(mbr_part_is_fat32(partition_run.part_type));

    fs = fat32_mk(&partition_run);
    root = fat32_get_root(&fs);

    printk("Got root\n");

    // default parameters, SET THEM HARDCODED FOR OUR PROJECT
    char *checkpoint_path = CHECKPOINT_PATH;  // e.g. out/model.bin
    char *tokenizer_path = TOKENIZER_PATH;
    float temperature = 1.0f;   // 0.0 = greedy deterministic. 1.0 = original. don't set higher
    float topp = 0.9f;          // top-p in nucleus sampling. 1.0 = off. 0.9 works well, but slower
    int steps = STEPS;            // number of steps to run for
    char *prompt = NULL;        // prompt string
    unsigned long long rng_seed = timer_get_usec(); // seed rng with time by default
    char *mode = "generate";    // generate|chat
    char *system_prompt = NULL; // the (optional) system prompt to use in chat mode

    printk("Got checkpoint path: \n");
    printk("%s\n", checkpoint_path);

    // parameter validation/overrides
    if (rng_seed <= 0) rng_seed = timer_get_usec();
    if (temperature < 0.0) temperature = 0.0;
    if (topp < 0.0 || 1.0 < topp) topp = 0.9;
    if (steps < 0) steps = 0;

    // build the Transformer via the model .bin file
    Transformer transformer;
    build_transformer(&transformer, checkpoint_path);
    if (steps == 0 || steps > transformer.config.seq_len) steps = transformer.config.seq_len; // override to ~max length

    printk("Built transformer\n");

    // build the Tokenizer via the tokenizer .bin file
    Tokenizer tokenizer;
    build_tokenizer(&tokenizer, tokenizer_path, transformer.config.vocab_size);

    printk("Built tokenizer\n");

    // build the Sampler
    Sampler sampler;
    build_sampler(&sampler, transformer.config.vocab_size, temperature, topp, rng_seed);

    printk("Built sampler\n");

    #if TIMER_PROFILING
    // enable collection of profiler data
    interrupt_init();
    timer_init(16, 0x100);
    gprof_init();
    // printk("gonna enable ints globally!\n");
    enable_interrupts();
    #endif

    // run!
    if (strcmp(mode, "generate") == 0) {
        printk("Generating...\n");
        generate(&transformer, &tokenizer, &sampler, prompt, steps);
        #if TIMER_PROFILING
        gprof_dump(10);
        #endif
    } 
    else {
        panic("unknown mode: %s\n", mode);
        // error_usage();
    }
    clean_reboot();
}
#endif
