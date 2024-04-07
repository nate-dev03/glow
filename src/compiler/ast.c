#include <stdlib.h>
#include <stdio.h>
#include "err.h"
#include "util.h"
#include "ast.h"

GlowAST *glow_ast_new(GlowNodeType type, GlowAST *left, GlowAST *right, unsigned int lineno)
{
	GlowAST *ast = glow_malloc(sizeof(GlowAST));
	ast->type = type;
	ast->lineno = lineno;
	ast->left = left;
	ast->right = right;
	return ast;
}

struct glow_ast_list *glow_ast_list_new(void)
{
	struct glow_ast_list *list = glow_malloc(sizeof(struct glow_ast_list));
	list->ast = NULL;
	list->next = NULL;
	return list;
}

void glow_ast_list_free(struct glow_ast_list *list)
{
	while (list != NULL) {
		struct glow_ast_list *temp = list;
		list = list->next;
		glow_ast_free(temp->ast);
		free(temp);
	}
}

void glow_ast_free(GlowAST *ast)
{
	if (ast == NULL)
		return;

	switch (ast->type) {
	case GLOW_NODE_STRING:
		glow_str_free(ast->v.str_val);
		break;
	case GLOW_NODE_IDENT:
		glow_str_free(ast->v.ident);
		break;
	case GLOW_NODE_IF:
	case GLOW_NODE_ELIF:
	case GLOW_NODE_FOR:
		glow_ast_free(ast->v.middle);
		break;
	case GLOW_NODE_DEF:
	case GLOW_NODE_GEN:
	case GLOW_NODE_ACT:
	case GLOW_NODE_CALL:
		glow_ast_list_free(ast->v.params);
		break;
	case GLOW_NODE_BLOCK:
		glow_ast_list_free(ast->v.block);
		break;
	case GLOW_NODE_LIST:
	case GLOW_NODE_TUPLE:
	case GLOW_NODE_SET:
	case GLOW_NODE_DICT:
		glow_ast_list_free(ast->v.list);
		break;
	case GLOW_NODE_TRY_CATCH:
		glow_ast_list_free(ast->v.excs);
		break;
	case GLOW_NODE_COND_EXPR:
		glow_ast_free(ast->v.middle);
		break;
	default:
		break;
	}

	GlowAST *left = ast->left, *right = ast->right;
	free(ast);
	glow_ast_free(left);
	glow_ast_free(right);
}
