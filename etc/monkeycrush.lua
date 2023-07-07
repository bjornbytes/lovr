-- Usage: lovr etc/monkeycrush.lua > etc/monkey.h

local etc = lovr.filesystem.getSource()
local success, model = assert(pcall(lovr.data.newModelData, 'monkey.glb'))

local min, max = lovr.math.newVec3(math.huge), lovr.math.newVec3(-math.huge)

for i = 1, model:getMeshVertexCount(1) do
  local x, y, z, nx, ny, nz = model:getMeshVertex(1, i)
  min.x, min.y, min.z = math.min(x, min.x), math.min(y, min.y), math.min(z, min.z)
  max.x, max.y, max.z = math.max(x, max.x), math.max(y, max.y), math.max(z, max.z)
end

local scale = .5
min:mul(scale)
max:mul(scale)

local center = Vec3(max + min):mul(.5)
local extent = Vec3(max - min)
local halfExtent = extent / 2
local bounds = { center[1], center[2], center[3], halfExtent[1], halfExtent[2], halfExtent[3] }

io.write(('float monkey_bounds[6] = { %ff, %ff, %ff, %ff, %ff, %ff };\n'):format(unpack(bounds)))
io.write(('float monkey_offset[3] = { %ff, %ff, %ff };\n'):format(min:unpack()))
io.write('\n')

io.write('uint8_t monkey_vertices[] = {\n')
for i = 1, model:getMeshVertexCount(1) do
  local x, y, z, nx, ny, nz = model:getMeshVertex(1, i)
  local position = vec3(x, y, z):mul(scale)
  local normal = vec3(nx, ny, nz)

  local qx, qy, qz = ((position - min) / extent * 255 + .5):unpack()
  local qnx, qny, qnz = ((normal / 2 + .5) * 255 + .5):unpack()

  qx, qy, qz = math.floor(qx), math.floor(qy), math.floor(qz)
  qnx, qny, qnz = math.floor(qnx), math.floor(qny), math.floor(qnz)

  io.write(('  %d, %d, %d, %d, %d, %d,\n'):format(qx, qy, qz, qnx, qny, qnz))

  lovr.math.drain()
end
io.write('};\n\n')

io.write('uint16_t monkey_indices[] = {\n ')
for i = 1, model:getMeshIndexCount(1) do
  local index = model:getMeshIndex(1, i) - 1
  io.write((' %d,'):format(index))
  if i % 10 == 0 then
    io.write('\n ')
  end
end
io.write('\n};\n')

lovr.event.quit()
