{main}
    // 1. Establish Memory
    {var} server_version = 1.2 {-var}
    {var} admin = AADI {-var}

    // 2. Open a Secure Public Tunnel 
    {tunnel port="8080"}{-tunnel}

    // 3. Start the Event Loop
    {https port="8080"}
        
        // Route 1: The Home Page (Serving HTML!)
        {route path="/"}
            {res}
                <h1>Welcome to JumboLang V$server_version</h1>
                <p>Status: ONLINE</p>
                <p>Your server is running beautifully.</p>
            {-res}
        {-route}

        // Route 2: The API Endpoint
        {route path="/api/user"}
            {res}
                <h3>Admin Profile: $admin</h3>
                <p>Role: Creator</p>
                <p>Power Level: Maximum</p>
            {-res}
        {-route}

    {-https}
{-main}