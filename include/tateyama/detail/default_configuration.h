inline constexpr char default_configuration[] {  // NOLINT
    "[jogasaki]\n"
        "thread_pool_size=5\n"
        "lazy_worker=false\n"

    "[ipc_endpoint]\n"
        "database_name=tateyama\n"
        "threads=104\n"

    "[stream_endpoint]\n"
        "port=12345\n"
        "threads=104\n"

    "[ogawayama]\n"
        "name=tateyama\n"
        "threads=104\n"
};
