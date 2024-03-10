add_executable(metaltoy 
    main.cpp
    app.cpp
    renderer.cpp
    metalimpl.cpp
)
target_link_libraries(metaltoy METAL_CPP)
