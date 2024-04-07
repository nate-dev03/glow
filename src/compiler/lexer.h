#ifndef GLOW_LEXER_H
#define GLOW_LEXER_H

#include <stdlib.h>
#include <stdbool.h>
#include "parser.h"

GlowToken *glow_parser_next_token(GlowParser *p);
GlowToken *glow_parser_next_token_direct(GlowParser *p);
GlowToken *glow_parser_peek_token(GlowParser *p);
GlowToken *glow_parser_peek_token_direct(GlowParser *p);
bool glow_parser_has_next_token(GlowParser *p);
void glow_parser_tokenize(GlowParser *p);
const char *glow_type_to_str(GlowTokType type);

#endif /* GLOW_LEXER_H */
