
# F2ARC Archiver
<img src="https://imgbly.com/ib/SwBFbYwU21.png">

Based on Huffman algorithm.
Основан на алгоритме Хаффмана.
# Interface
```$ ftarc.exe -[axdltfnh] archive[.f2arc] [/path/to/] file_1 file_2 ...```
- ```a``` - поместить файл(ы) в архив;

   ```$ ftarc.exe -a arc file_1 path/to/file/file_2```

```x``` - извлечь файл(ы) из архива;
- ```$ ftarc.exe -x arc``` - Extract all files
- ```$ ftarc.exe arc -xf / file1 "file 2.txt"```

  ```$ ftarc.exe -xf arc C:\myfolder\ file1 "file 2.txt"``` - Extract files with specified names
- ```$ ftarc.exe -x arc -n 0 2 4```

  ```$ ftarc.exe -xn arc 0 2 4``` - Extract files with id 0, 2, 4

```d``` - удалить файл(ы) из архива;
- similar to extract

```l``` - вывести информацию о файлах, хранящихся в архиве;
```t``` - проверить целостность архива.

```$ ftarc.exe help ``` ```$ ftarc.exe -h ```
Show manual
## Main logic
``` archiver.c ```
```c
Archive* archive_open(Str file_name); // open existing archive 
Archive* archive_new(Str file_name); // create new archive
void archive_add_file(Archive* arc, Str file_name); // add file
DynListArchiveFile* archive_get_files(Archive* arc); // get files metadata
```

## Archive structure
<img src="https://imgbly.com/ib/vP4P5BN1rP.png">

