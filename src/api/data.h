#include "data/blob.h"
#include "data/vertexData.h"
#include <stdbool.h>

int luax_loadvertices(lua_State* L, int index, VertexFormat* format, AttributePointer vertices);
bool luax_checkvertexformat(lua_State* L, int index, VertexFormat* format);
int luax_pushvertexformat(lua_State* L, VertexFormat* format);
int luax_pushvertexattribute(lua_State* L, AttributePointer* vertex, Attribute attribute);
int luax_pushvertex(lua_State* L, AttributePointer* vertex, VertexFormat* format);
void luax_setvertexattribute(lua_State* L, int index, AttributePointer* vertex, Attribute attribute);
void luax_setvertex(lua_State* L, int index, AttributePointer* vertex, VertexFormat* format);
Blob* luax_readblob(lua_State* L, int index, const char* debug);
