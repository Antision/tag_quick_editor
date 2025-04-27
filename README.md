# Tag Quick Editor

Tag Quick Editor is a software designed for editing training dataset captions in Danbooru-style tag format. It includes built-in editor modules for capturing specific tags, primarily used to manually correct tags that WD14 Tagger often misidentifies. The software design references [BooruDatasetTagManager](https://github.com/starik222/BooruDatasetTagManager/tree/master).

## License
This project is licensed under the [GPL-3.0 License](LICENSE).

## Important Notes
1. **Raw Display**: The program does not hide bracket escapes or weights; escaped brackets will be displayed in their original form.
2. **Tag Separators**: When reading tag text, the program uses commas, periods, and newlines as separators. However, all separators will be converted to commas upon saving.
3. **Auto-Merge Tags**: The program automatically merges certain tags. To disable this, uncheck **Options -> Auto Merge Tags** in the menu bar.
4. **Beta Stage**: This software is in testing and stability is not guaranteed. Please report bugs via GitHub.

## Basic Usage
### Opening Folders
Click **File -> Open** in the menu bar.

## Editor Modules
The rightmost column in the interface contains editor modules, which enable rapid tag editing via mouse interactions.

### Auto-Merge Behavior
Editors may auto-merge tags by default. Disable this via **Options -> Auto Merge Tags**. Settings are persisted.

#### Hair & Eyes Editor
The first tag list in this editor merges hair colors to handle highlights/gradients:
- `[color] streaked hair` merges with `[color] hair` or `[color] hair with [color] ... streaks`.
- `[color] gradient hair` merges with `[color] gradient hair` or `gradient from ... to ...`.
- When multiple mergeable tags exist, the **last** tag in the list takes priority. Reorder tags via drag-and-drop (within the editor's list, not the main tag list).

**Example 1 (Highlights):**  
Tags: `streaked hair`, `white hair`, `blue hair`  
1. In the Hair & Color editor, click the `[s]` button next to `white hair` → becomes `white streaked hair`.  
2. The editor merges it with `blue hair` → `blue hair with white streaks`.

**Example 2 (Gradient):**  
Tags: `gradient hair`, `white hair`, `green hair`  
1. Reorder tags: place `white hair` above `green hair`.  
2. Click `[g]` on both → merged into `gradient from white to green`.

#### Clothes Editor
The first list auto-merges clothing tags under these conditions:  
1. Color tags remain at the front.  
2. Tags with different colors are not merged.

## Image Browsing
- Double-click an image in the list to preview.  
- Use arrow keys to navigate or press `R` to reset zoom.

## Multi-Select Operations
### Tags/Images
- **Images**: Use `Shift`/`Ctrl` or drag to multi-select.  
- **Multi-Edit Mode**: When multiple images are selected, tags represent aggregated tags across all selected images.  

**Bulk Tag Positioning**:  
1. Drag a tag (e.g., `hat`) to a new position in the multi-edit list.  
2. Right-click → `SetPos` → moves the tag to that relative position in **all** selected images.

## Image Filtering
Open **Tools -> Filter** and input tags (press `Enter` after each). Matching images will appear in the filter window.

## State Saving
To prevent data loss during crashes:  
- Enable **Auto Save State** (default: on) → edits are saved to `runtime_state.json` every 20 seconds.  
- On restart, the program asks whether to recover unsaved states.

## Tag Data Updates
Auto-completion relies on `tag.tag`, converted from [danbooru_e621_merged.csv](https://github.com/DominikDoom/a1111-sd-webui-tagcomplete/blob/main/tags/danbooru_e621_merged.csv).  
- If `tag.tag` is missing, the program downloads the latest CSV and converts it.  
- **Manual Update**: Delete `tag.tag` to force a refresh.

## Updates
The program checks for updates on startup. If a new version is detected:  
1. Your browser will open to the GitHub Releases page.  
2. Download the latest version and overwrite existing files in the installation directory.