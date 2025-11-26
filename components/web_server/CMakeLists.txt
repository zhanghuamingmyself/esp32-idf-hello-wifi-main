# components/web_server/CMakeLists.txt

set(COMPONENT_NAME web_server)


idf_component_register(SRCS
    "web_server.c"
    
    INCLUDE_DIRS
        "include"
    
    REQUIRES
        esp_http_server
        spiffs
        json
        vfs
)

# 嵌入网页文件

set(web_files
    "data/index.html"
)
    
foreach(web_file ${web_files})
    get_filename_component(web_file_name ${web_file} NAME)
    string(REPLACE "." "_" web_file_var ${web_file_name})
    target_add_binary_data(${COMPONENT_TARGET} "${web_file}" TEXT)
endforeach()