Contributing
===

You want to contribute to LÖVR?  That's awesome!

Submitting Issues
---

Feel free to file an issue if you notice a bug.  Make sure you search before filing an issue, as it
may have already been asked.

Issues are okay for feature requests and questions about the development of LÖVR as well, but
usually you'll get a better response by asking in
[Slack](https://join.slack.com/ifyouwannabemylovr/shared_invite/MTc5ODk2MjE0NDM3LTE0OTQxMTIyMDEtMzdhOGVlODFhYg).
Questions about how to use LÖVR belong in Slack.

Contributing Documentation
---

If you see any typos or inconsistencies in the docs, submitting an issue or pull request in the
[lovr-docs repo](https://github.com/bjornbytes/lovr-docs) would be greatly appreciated!  The `api`
folder has Lua files for each function, the `guides` folder contains tutorials in markdown format,
and the `examples` folder has source code for sample projects and other demos.

Contributing Code
---

To contribute patches, fork LÖVR, commit to a branch, and submit a pull request.  For larger
changes, it can be a good idea to engage in initial discussion via issues or Slack before
submitting.  Try to stick to the existing coding style:

- 2 space indentation.
- 100 character wrapping (ish, sometimes it's more readable to just have a long line).
- Use descriptive, camelCased names when possible.

If you modify the embedded `boot.lua` script, you can compile it into a C header by doing this:

```sh
pushd src/resources; xxd -i boot.lua > boot.lua.h; popd
```

Organization
---

An overview of the folder structure:

- `deps` contains submodules for external dependencies.  Some smaller dependencies are also included
in the `src/lib` folder.
- `src/api` contains Lua bindings.  There's a file for each module in there, and a `types` folder
with a file for each type of object.
- `src/resources` contains embedded resources.  These are compiled to binary headers using `xxd`.
- `src/data` has stuff for loading various file formats into LÖVR data structures.
- Each module has a folder in `src`.
