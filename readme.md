
# F2ARC Archiver

Based on Huffman algorithm.

Основан на алгоритме Хаффмана.

# Interface
```$ ftarc.exe -[axdltfnho] archive[.f2a] [/path/to/] file_1 file_2 ...```
- ```a``` - поместить файл(ы) в архив / создать архив;

   ```$ ftarc.exe -a arc file_1 path/to/file/file_2 c:/file3.txt```
  
  ```ao``` - перезаписать/создать архив

```x``` - извлечь файл(ы) из архива;
- ```$ ftarc.exe -x arc .``` - Извлечь все файлы в текущую папку
- ```$ ftarc.exe arc -xf local/path file1 "file 2.txt"```

  ```$ ftarc.exe -xf arc C:\myfolder\ file1 "file 2.txt"``` - Извлечь файлы с определенными именами

  ```$ ftarc.exe -xn arc 0 2 4``` - Извлечь файлы с номерами 0, 2, 4

```d``` - удалить файл(ы) из архива;
- Аналогично извлечению, только без выходного пути

```l``` - вывести информацию о файлах, хранящихся в архиве;

```t``` - проверить целостность архива.

```$ ftarc.exe  ``` ```$ ftarc.exe -h ```
Показать мануал
## Main logic
``` archiver.h ```
```c
Archive* archive_open(Str file_name, bool write_mode); // Открыть существующий архив 
Archive* archive_new(Str file_name, bool override); // Создать новый архив

void archive_add_file(Archive* arc, Str file_name); // Добавить файл
DynListArchiveFile* archive_get_files(Archive* arc); // Получить информацию о файлах

void archive_save(Archive* arc); // Сохранить файлы, добавленные archive_add_file()
void archive_extract(Archive* arc, Str out_path, DynListInt* files_ids);
void archive_remove_files(Archive* arc, DynListInt* files_ids);
bool archive_validate(Archive* arc);

```

## Archive structure
<img src="https://github.com/zzz3230/ftarc/blob/master/f2arc_architect.png?raw=true">

