#include "graphics/canvas.h"
#include "graphics/shader.h"
#include "graphics/texture.h"

int luax_checkuniform(lua_State* L, int index, const Uniform* uniform, void* dest, const char* debug);
void luax_checkuniformtype(lua_State* L, int index, UniformType* baseType, int* components);
int luax_optmipmap(lua_State* L, int index, Texture* texture);
Texture* luax_checktexture(lua_State* L, int index);
void luax_readattachments(lua_State* L, int index, Attachment* attachments, int* count);
