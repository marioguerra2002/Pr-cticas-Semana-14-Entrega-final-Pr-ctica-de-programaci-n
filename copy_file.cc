#include <cstdlib>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <system_error>
#include <libgen.h>
#include <fcntl.h>
#include <cassert>
#include <limits.h>
#include "scope.hpp"

#include <stdexcept>

#include <vector> 
#include <string>
// std::string dirname(const std::string& path) {
//   std::array<char, PATH_MAX> buffer;
//   path.copy(buffer.data(), buffer.size());
//   std::cout << "El directorio de " << path << " es " << dirname(buffer.data()) << std::endl;
//   return std::string(buffer.data());
// }
// std::error_code write (int fd, std::array<uint8_t, 240>& buffer) {
//   ssize_t bytes_read = write(fd, buffer.data(), buffer.size());
//   while (bytes_read > 0) {
//     write(fd, buffer.data(), bytes_read); // se escribe en el archivo
//     bytes_read = read(fd, buffer.data(), buffer.size()); // se continua leyendo el archivo (se lee 240 bytes mas)
//   }
//   if (bytes_read < 0) { // si se produce un error
//     return std::error_code(errno, std::system_category());
//   }
//   return std::error_code(); // si no se produce ningun error
// }
std::error_code error_in_copy_file(const std::string& src_path, const std::string& dst_path, bool preserve_all) {
  int src_fd = open(src_path.c_str(), O_RDONLY);
  struct stat src_stat_check; // se crea una estructura para verificar si el archivo existe
  if (stat(src_path.c_str(), &src_stat_check) != 0) { // se verifica si el archivo existe
    // std::cout << "El archivo no existe" << std::endl;
    return std::error_code(errno, std::system_category()); // se retorna el error y el codigo de error
  }
  if (S_ISREG(src_fd)) { // se verifica si es un archivo regular
    return std::error_code(ENOTSUP, std::system_category()); // si no es un archivo regular, se retorna el error
  }
  //auto src_guard = scope::scope_exit([&src_fd]() { close(src_fd); }); // se cierra el archivo
  int dst_fd = open(dst_path.c_str(), O_WRONLY);  // se abre el archivo destino
  struct stat dst_stat_check; // se crea una estructura para verificar si el archivo existe
  if (stat(dst_path.c_str(), &dst_stat_check) == 0) { // se verifica si el archivo existe
    //// Comprobar que los archivos no son el mismo
    struct stat src_stat, dst_stat;
    stat (src_path.c_str(), &src_stat);
    stat (dst_path.c_str(), &dst_stat);
    // se compara el inode y el device id para verificar que no son el mismo archivo
    // intento de gestion de exepciones (no funciona)
    try {
      assert(src_stat.st_ino != dst_stat.st_ino || src_stat.st_dev != dst_stat.st_dev);
    } catch (const std::logic_error& e) {
      std::cout << "Los dos archivos son iguales" << std::endl;
      return std::error_code(EEXIST, std::system_category()); // se retorna el error y el codigo de error 
    }
    if (S_ISDIR(dst_stat.st_mode)) { // se verifica si es un directorio
      // guardar en dst_path una nueva ruta con el nombre de archivo de src_path en el directorio dst_path
      std::string dst_path_copy;
      if (dst_path[dst_path.size() - 1] != '/') {
        dst_path_copy = dst_path + "/" + std::string(const_cast<char*>(src_path.c_str())); // se crea la nueva ruta
        dst_fd = open(dst_path_copy.c_str(), O_WRONLY | O_CREAT, 0644); // creamos el archivo en la nueva ruta
      } else {
        dst_path_copy = dst_path + std::string(const_cast<char*>(src_path.c_str()));
        dst_fd = open(dst_path_copy.c_str(), O_WRONLY | O_CREAT, 0644);
      }
    }
  }
  close (src_fd);  
  close (dst_fd);
  /////
  // abrimos src_path en modo lectura
  int src_fd_2 = open(src_path.c_str(), O_RDONLY);
  // abrimos dst_path en modo escritura
  int dst_fd_2 = open(dst_path.c_str(), O_WRONLY);
  struct stat dst_stat_check_2; // se crea una estructura para verificar si el archivo existe
  if (stat(dst_path.c_str(), &dst_stat_check_2) == 0) { // se verifica si el archivo existe
    // abrimos y vaciamos dst_path
    dst_fd_2 = open(dst_path.c_str(), O_RDWR | O_TRUNC);
    
  } else {
    // creamos dst_path
    dst_fd_2 = open(dst_path.c_str(), O_RDWR | O_CREAT, 0666);

  }
  if (dst_fd_2 < 0) { // comprobamos si se ha producido un error al abrir el archivo o crearlo
    return std::error_code(errno, std::system_category());
  }
  /// copiar src_path en dst_path
  std::array<uint8_t, 240> buffer; // se crea un buffer de 240 bytes
  ssize_t bytes_read = read(src_fd_2, buffer.data(), buffer.size()); // se lee el archivo (se lee 240 bytes)
  if (bytes_read > buffer.size()) {
    return std::error_code(errno, std::system_category());
  }
  while (bytes_read > 0) {  // mientras se sigan leyendo bytes
    write(dst_fd_2, buffer.data(), bytes_read); // se escribe en el archivo
    bytes_read = read(src_fd_2, buffer.data(), buffer.size()); // se continua leyendo el archivo (se lee 240 bytes mas)
  }
  // en caso de que write tenga un error, que es cuando aún quedan cosas por escribir en el buffer
  if (buffer.size() != 0) { // si no se leyeron 240 bytes
    write(dst_fd_2, buffer.data(), bytes_read); // se escribe lo que se haya leido
  }
  /// en caso de que se quiera copiar atributos de src_path a dst_path
  if (preserve_all == true) { // si se quiere preservar todos los atributos
    struct stat src_stat;
    stat (src_path.c_str(), &src_stat);
    // se copian los atributos del archivo src_path al archivo dst_path
    fchmod(dst_fd_2, src_stat.st_mode); // se copian los permisos
    fchown(dst_fd_2, src_stat.st_uid, src_stat.st_gid); // se copia el usuario y el grupo
    struct timespec times[2]; // se crea una estructura para copiar las fechas de acceso y modificación
    times[0] = src_stat.st_atim; // se copia la fecha de acceso
    times[1] = src_stat.st_mtim; // se copia la fecha de modificación
    futimens(dst_fd_2, times); // se copian las fechas de acceso y modificación
  }
  close (src_fd_2);
  close (dst_fd_2);
  return std::error_code(0, std::system_category()); // en caso de que no haya errores, se retorna un error vacio
}

std::error_code error_in_move_file(const std::string& src_path, const std::string& dst_path) {
  int src_fd = open(src_path.c_str(), O_RDONLY);
  struct stat src_stat; // se crea una estructura para verificar si el archivo existe
  if (stat(src_path.c_str(), &src_stat) != 0) { // se verifica si el archivo existe
    // std::cout << "El archivo no existe" << std::endl;
    return std::error_code(errno, std::system_category()); // se retorna el error y el codigo de error
  }
  if (!S_ISREG(src_stat.st_mode)) { // se verifica si el archivo es un archivo regular
    // std::cout << "El archivo es un archivo regular" << std::endl;
    return std::error_code(errno, std::system_category()); // se retorna el error y el codigo de error
  }
  int dst_fd = open(dst_path.c_str(), O_WRONLY);  // se abre el archivo destino
  struct stat dst_stat; // se crea una estructura para verificar si el archivo existe
  if (S_ISDIR(dst_stat.st_mode)) { // si es un directorio
    std::string dst_path_copy; // se crea una variable para la nueva ruta
    if (dst_path[dst_path.size() - 1] != '/') {
      dst_path_copy = dst_path + "/" + std::string(const_cast<char*>(src_path.c_str())); // se crea la nueva ruta
      dst_fd = open(dst_path_copy.c_str(), O_WRONLY | O_CREAT, 0644); // creamos el archivo en la nueva ruta
    } else {
      dst_path_copy = dst_path + std::string(const_cast<char*>(src_path.c_str()));
      dst_fd = open(dst_path_copy.c_str(), O_WRONLY | O_CREAT, 0644);
    }
  }
  stat (dst_path.c_str(), &dst_stat); // se verifica si el archivo existe
  stat (src_path.c_str(), &src_stat); // se verifica si el archivo existe
  if (src_stat.st_dev == dst_stat.st_dev) {
    std::rename (src_path.c_str(), dst_path.c_str()); // se renombra el archivo
    ///// tengo que ejecutar dos veces para que se borre por asi decirlo el anteior
    int erase = unlink(src_path.c_str()); // se elimina el archivo ///// no funciona
  } else {
    // si no están en el mismo dispositivo
    std::array<uint8_t, 240> buffer; // se crea un buffer de 240 bytes
    ssize_t bytes_read = read(src_fd, buffer.data(), buffer.size()); // se lee el archivo (se lee 240 bytes)
    if (bytes_read > buffer.size()) {
    return std::error_code(errno, std::system_category());
    }
    while (bytes_read > 0) {  // mientras se sigan leyendo bytes
      write(dst_fd, buffer.data(), bytes_read); // se escribe en el archivo
      bytes_read = read(src_fd, buffer.data(), buffer.size()); // se continua leyendo el archivo (se lee 240 bytes mas)
    }
    // en caso de que write tenga un error, que es cuando aún quedan cosas por escribir en el buffer
    if (buffer.size() != 0) { // si no se leyeron 240 bytes
      write(dst_fd, buffer.data(), bytes_read); // se escribe lo que se haya leido
    }
    fchmod(dst_fd, src_stat.st_mode); // se copian los permisos
    fchown(dst_fd, src_stat.st_uid, src_stat.st_gid); // se copia el usuario y el grupo
    struct timespec times[2]; // se crea una estructura para copiar las fechas de acceso y modificación
    times[0] = src_stat.st_atim; // se copia la fecha de acceso
    times[1] = src_stat.st_mtim; // se copia la fecha de modificación
    futimens(dst_fd, times); // se copian las fechas de acceso y modificación
    // borrar src_path
    int erase = unlink(src_path.c_str()); // se elimina el archivo 
  }
  close (src_fd);
  close (dst_fd);
  return std::error_code(0, std::system_category()); // en caso de que no haya errores, se retorna un error vacio
}
int main (int argc, char *argv[]) {
  std::string src_path;
  std::string dst_path;
  std::string presserve_all_text;
  bool preserve_all=false;
  if (argc == 4) {
    src_path = argv[2];
    dst_path = argv[3];
    presserve_all_text = argv[1];
    if (presserve_all_text == "-a") {
      preserve_all = true;
      std::error_code error = error_in_copy_file(src_path, dst_path, preserve_all);
      if (error) { // en caso de que haya un error, se imprime el error
        std::cout << "Error: " << error.message() << std::endl;
        return 1;
      }
    } else if (presserve_all_text == "-m") {
      std::error_code error = error_in_move_file(src_path, dst_path);
      if (error) { // en caso de que haya un error, se imprime el error
        std::cout << "Error: " << error.message() << std::endl;
        return 1;
      }
    } else if (presserve_all_text != "-a" || presserve_all_text != "-m") {
      std::cout << "Error: Argumento incorrecto" << std::endl;
      return 1;
    }
  } else if (argc == 3) {
    src_path = argv[1];
    dst_path = argv[2];
  } else {
    std::cout << "Error: Numero de argumentos incorrecto" << std::endl;
    return 1;
  }
  std::error_code error = error_in_copy_file(src_path, dst_path, preserve_all);
  if (error) { // en caso de que haya un error, se imprime el error
    std::cout << "Error: " << error.message() << std::endl;
  }
  // dirname(dst_path);
}