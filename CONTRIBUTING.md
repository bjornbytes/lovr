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
Questions about how to use LÖVR should go in Slack.

Editing Documentation
---

If you notice any typos or inconsistencies in the docs, speak up!  You can mention it in Slack, or
if you're feeling brave you can submit a pull request in the [lovr-docs repo](https://github.com/bjornbytes/lovr-docs).

Contributing Code
---

To contribute patches to the C code, you can fork LÖVR, commit to a branch, and submit a pull
request.  Note that contributions to the repository will be released under the terms in the LICENSE.
For larger changes, it is a good idea to engage in initial discussion via issues or Slack before
submitting.  Try to stick to the existing coding style:

- 2 space indentation.
- 100 character wrapping (ish, sometimes it's more readable to just have a long line).
- Use descriptive, camelCased variable names when possible.

If you modify the embedded `boot.lua` script, you can compile it into a C header by doing this:

```sh
xxd -i src/resources/boot.lua > src/resources/boot.lua.h
```

Organization
---

An overview of the folder structure:

- `deps` contains submodules for external dependencies.  Some smaller dependencies are also included
in the `src/lib` folder.
- `src/api` contains Lua bindings.  There's a file for each module and a file for each object.
- `src/core` contains shared engine code.  It's usually lower-level and not specific to LÖVR.
- `src/lib` contains smaller third party libraries.
- `src/modules` has a folder for each module in the project.  It's good to keep them separated as
  much as possible, there's inevitably some overlap.
- `src/resources` contains embedded files.  These are compiled to binary headers using `xxd`.

Branches other than master can be force-pushed during development to organize commit history.
