#include "python_interpreter.h"
#include "notebook.h"
#include "config.h"
#include <iostream>
#include <pygobject.h>
#include "menu.h"
#include "directories.h"

inline pybind11::module pyobject_from_gobj(gpointer ptr){
  auto obj=G_OBJECT(ptr);
  if(obj)
    return pybind11::module(pygobject_new(obj), false);
  return pybind11::module(Py_None, false);
}

Python::Interpreter::Interpreter(){
  auto init_juci_api=[](){
    pybind11::module(pygobject_init(-1,-1,-1),false);
    pybind11::module api("jucpp","Python bindings for juCi++");
    api.def("get_juci_home",[](){return Config::get().juci_home_path().string();})
    .def("get_plugin_folder",[](){return Config::get().python.plugin_directory;})
    .def("get_current_gtk_source_view",[](){
      auto view=Notebook::get().get_current_view();
      if(view)
        return pyobject_from_gobj(view->gobj());
      return pybind11::module(Py_None,false);
    })
    .def("get_gio_plugin_menu",[](){
      auto &plugin_menu=Menu::get().plugin_menu;
      if(!plugin_menu){
        plugin_menu=Gio::Menu::create();
        plugin_menu->append("<empty>");
        Menu::get().window_menu->append_submenu("_Plugins",plugin_menu);
      }
      return pyobject_from_gobj(plugin_menu->gobj());
    })
    .def("get_gio_window_menu",[](){return pyobject_from_gobj(Menu::get().window_menu->gobj());})
    .def("get_gio_juci_menu",[](){return pyobject_from_gobj(Menu::get().juci_menu->gobj());})
    .def("get_gtk_notebook",[](){return pyobject_from_gobj(Notebook::get().gobj());})
    .def_submodule("terminal")
      .def("get_gtk_text_view",[](){return pyobject_from_gobj(Terminal::get().gobj());})
      .def("println", [](const std::string &message){ Terminal::get().print(message +"\n"); });
    api.def_submodule("directories")
    .def("get_gtk_treeview",[](){return pyobject_from_gobj(Directories::get().gobj());})
    .def("open",[](const std::string &directory){Directories::get().open(directory);})
    .def("update",[](){Directories::get().update();});
    return api.ptr();
  };
  PyImport_AppendInittab("jucipp", init_juci_api);
  Config::get().load();
  auto plugin_path=Config::get().python.plugin_directory;
  add_path(Config::get().python.site_packages);
  add_path(plugin_path);
  Py_Initialize();
  boost::filesystem::directory_iterator end_it;
  for(boost::filesystem::directory_iterator it(plugin_path);it!=end_it;it++){
    auto module_name=it->path().stem().string();
    if(module_name!="__pycache__"){
      auto module=import(module_name);
      if(!module)
        std::cerr << std::string(Error()) << std::endl;
    }
  }
}

pybind11::module Python::Interpreter::get_loaded_module(const std::string &module_name){
  return pybind11::module(PyImport_AddModule(module_name.c_str()), true);
}

pybind11::module Python::Interpreter::import(const std::string &module_name){
  return pybind11::module(PyImport_ImportModule(module_name.c_str()), false);
}

void Python::Interpreter::add_path(const boost::filesystem::path &path){
  std::wstring sys_path(Py_GetPath());
  if(!sys_path.empty())
#ifdef _WIN32
    sys_path += ';';
#else
    sys_path += ':';
#endif
  sys_path += path.generic_wstring();
  Py_SetPath(sys_path.c_str());
}

Python::Interpreter::~Interpreter(){
  auto err=Error();
  if(Py_IsInitialized())
    Py_Finalize();
  if(err)
    std::cerr << std::string(err) << std::endl;
}

Python::Error::Error(){
  pybind11::object error(PyErr_Occurred(), false);
  if(error){
    PyObject *exception,*value,*traceback;
    PyErr_Fetch(&exception,&value,&traceback);
    PyErr_NormalizeException(&exception,&value,&traceback);
    try{
      exp=std::string(pybind11::object(exception,false).str());
      val=std::string(pybind11::object(value,false).str());
      trace=std::string(pybind11::object(traceback,false).str());
    } catch (const std::runtime_error &e){
      exp=e.what();
    }
  }
}

Python::Error::operator std::string(){
  return exp + "\n" + val + "\n" + trace;
}

Python::Error::operator bool(){
  return !exp.empty();
}
