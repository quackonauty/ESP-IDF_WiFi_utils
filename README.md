# Wi-Fi Utilities

This project provides utilities for setting up Wi‑Fi station mode on the ESP32 with HTTPS client and server capabilities. It also includes an optional feature for uploading HTML (or other static) files to the ESP32’s flash memory using LittleFS.

## I. Simple Wi-Fi station mode
### HTTPS client
If you plan to use HTTPS requests, you must include the server certificates in your project. Follow these steps:

1. Create a Certificate Folder
    Create a folder (for example, `main/certs/client/`) to store the server certificates.

2. Register the Certificate
    Add the certificate to your component configuration by including it with the idf_component_register macro. For example:

    ``` txt
    idf_component_register(SRCS "main.c"
                        INCLUDE_DIRS "."
                        EMBED_TXTFILES certs/client/yourservercert.pem)
    ```

3. Obtain a Server Certificate
    For instance, if you need the certificate for https://api.telegram.org, you can use OpenSSL to obtain it. Run the following command:

    ``` sh
        openssl s_client -connect api.telegram.org:443 -showcerts
    ```
### HTTPS server

To create an HTTPS server, you must include both the server certificate and the private key.

1. Prepare the Certificates

    - Place your server certificate and private key in the folder `main/certs/server`.
    - Name the files servercert.pem and prvtkey.pem, respectively.
  
2. Register the Server Files
    Include them in your component configuration as shown:

    ``` txt
        idf_component_register(SRCS "main.c"
                            INCLUDE_DIRS "."
                            EMBED_TXTFILES certs/client/yourservercert.pem
                            certs/server/servercert.pem certs/server/prvtkey.pem)
    ```

3. Generating Certificates with OpenSSL
    If you are not sure how to generate a certificate, you can use OpenSSL on Windows by following these steps:

    - Create a Configuration File
        Create a file named openssl-san.cnf (in the server certificates folder or any folder you prefer) with the following content:
        ``` cnf
            [ req ]
            default_bits        = 2048
            default_keyfile     = privkey.pem
            distinguished_name  = req_distinguished_name
            req_extensions      = v3_req
            x509_extensions     = v3_req

            [ req_distinguished_name ]
            organizationName    = example
            CN = <IP address or local DNS name>

            [ v3_req ]
            subjectAltName = @alt_names

            [ alt_names ]
            DNS.1 = <IP address or local DNS name>
        ```

    - Generate the Certificates
        Navigate to the folder containing openssl-san.cnf and run:

        ``` sh
            openssl req -x509 -new -nodes -keyout prvtkey.pem -out servercert.pem -days 365 -subj "/O=yourorganization/CN=<ip address>" -config openssl-san.cnf

            openssl x509 -outform der -in servercert.pem -out servercert.crt
        ```

    - Distribute the Certificate
        Distribute the generated servercert.crt to any client device that needs to connect to the secure server. Do not share these credentials with unauthorized parties.

4. Using mDNS for Dynamic DNS Naming
    Generating a certificate for every different IP address can be inconvenient. Instead, it is recommended to generate a certificate with a DNS name. To achieve this, follow these steps:

    - Add the mDNS Component
        Create an idf_component.yml file in your main project folder with the following content:
        ``` yml
        dependencies:
        espressif/mdns: "*"
        ```
        Then, rebuild your project so that the mDNS component is added.

    - Set Up the DNS Name
        In your code (after the device is connected to the network), initialize mDNS and set the hostname and instance name as follows:
        ``` C
        ESP_ERROR_CHECK(mdns_init());
        ESP_ERROR_CHECK(mdns_hostname_set(hostname));
        ESP_ERROR_CHECK(mdns_instance_name_set(instance_name));
        ```
    - Generate Certificates Using the DNS Name
        With a DNS name in place, you can generate your server’s .pem certificate and private key files. This method ensures that the certificate remains valid even if the IP address changes.

    Another thing to keep in mind is the HTTP header size. In HTTPS cases, you might need to increase the header size to accommodate larger request headers. You can adjust this setting in the SDK configuration editor, as shown in the image below:

    ![example1](/md/img/sdk_configuration_editor_max_hhtp_uri_legth.png)

### _Optional: Upload HTML Files to Flash Using LittleFS_
This optional feature allows you to store and serve HTML or other static files directly from the ESP32’s flash memory using LittleFS.

1. Add LittleFS Dependency
    Create an `idf_component.yml` file in your main project folder with the following content:
    ```yml
    dependencies:
      joltwallet/littlefs: "*"
    ```
    This will create a folder called `managed_components` that stores all your dependencies from the ESP Component Registry.

2. Create a Custom Partition Table
    Create a custom partition table CSV file (for example, `partitions.csv`) to assign flash memory partitions. Since the default partition table is flashed at offset `0x8000` and occupies `0x1000`, start your custom partitions at offset `0x9000`. The partition layout is similar to the built-in "Single factory app, no OTA" table, except that you add a 1MB data storage partition for LittleFS:

    ``` csv
    # Name   ,Type ,SubType  ,Offset ,Size   ,Flags
    nvs      ,data ,nvs      ,0x9000 ,0x6000 ,
    phy_init ,data ,phy      ,       ,0x1000 ,
    factory  ,app  ,factory  ,       ,1M     ,
    storage  ,data ,littlefs ,       ,1M     ,
    ```

3. Configure the SDK
    Using the SDK Configuration Editor:
    - Set the ESP32 to use your custom partition table.
    - Add the folder (e.g., `littlefs`) that will contain all the files to be uploaded.

    ![example1](/md/img/sdk_configuration_editor_partition_table.png)

    - Note: If your ESP32 has more than 2MB of flash, configure the SDK to use the full 4MB. Refer to the images below for guidance:
    - 
    ![example1](/md/img/sdk_configuration_editor_serial_flasher_config.png)

4. Organize Your Project Structure
    Your project should have a structure similar to this:

    ```
    ├── littlefs
    │   └── [files to be uploaded to the ESP32]
    ├── main
    │   ├── CMakeLists.txt
    │   ├── idf_component.yml
    │   └── main.c
    ├── managed_components
    │   └── [component folders]
    ├── partitions.csv
    └── [other files...]
    ```
5. Modify the CMakeLists.txt
   To instruct the ESP32 to upload the files from the `littlefs` folder, add the following line to the `CMakeLists.txt` file in your `main` folder:
