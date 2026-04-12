# Release Process

1. Update the version in `CMakeLists.txt`
2. Commit the version bump
3. Tag the commit:
   ```
   git tag v<major>.<minor>.<patch>
   ```
4. Push the tag:
   ```
   git push origin v<major>.<minor>.<patch>
   ```

The release workflow will build, test, and create a GitHub release
with auto-generated notes.
