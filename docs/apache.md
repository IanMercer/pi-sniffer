# Apache Web Server

Install Apache2 and configure it:

````
sudo apt-get install apache2

# Enable headers for CORS
sudo a2enmod headers

# Enable CGI
sudo a2enmod cgi

sudo systemctl restart apache2.service
````

Check that it works and displays the default apache website.

A sample ReactJS website is included and is shipped in a pre-built state, see below for instructions on how to edit and build it to suit your needs but
for now lets get the stock version running.


First link it into the location where Apache expects to find it under `/var/www`:

````
sudo ln -s ~/pi-sniffer/react/build /var/www/react

````

Next create or edit a "sites-enabled" definition for Apache2.

````
sudo nano /etc/apache2/sites-available/000-default.conf
````

Change `DocumentRoot` and allow cors:

````
        DocumentRoot /var/www/react
        Header set Access-Control-Allow-Origin "*"
````

See below for what this should look like when completed. NB The inclusion of a CORS setting allows the API to be called from 
other domains / ports which is important if you want to run the web site in development mode.


# CGI 

To build the simple CGI script that exposes the summary data as JSON, run `make cgijson`

This will also copy the `cgijson.cgi` file to the correct location on disk.

If the main service is running, check that it works `./cgijson` should display a JSON formatted
string containing the current system state. If it's not running you should get a message to that effect.

To copy it over manually you can use it `sudo cp cgijson /usr/lib/cgi-bin/cgijson.cgi


# Editing the web site

Once you have everything running you can edit the website to suit your needs, or discard it and build you own, all you really need is the
API call to get the data about how many people are in each group or room.

# Dependencies

Install all of the required depencies:

node v12+ includes npm

reactjs

react starter

font awesome

````
npm i --save @fortawesome/fontawesome-svg-core
  npm install --save @fortawesome/free-solid-svg-icons
  npm install --save @fortawesome/react-fontawesome
````

During development run `npm run start` in a separate session and access the site on port :3000.

Once you are done, build the website `npm run build` for production and then copy (or link) it into `/var/wwww/react` if you haven't already linked it as described above.


## Apache configuration: /etc/apache2/sites-available/000-default.conf
````

<VirtualHost *:80>
        # The ServerName directive sets the request scheme, hostname and port that
        # the server uses to identify itself. This is used when creating
        # redirection URLs. In the context of virtual hosts, the ServerName
        # specifies what hostname must appear in the request's Host: header to
        # match this virtual host. For the default virtual host (this file) this
        # value is not decisive as it is used as a last resort host regardless.
        # However, you must set it for any further virtual host explicitly.
        #ServerName www.example.com

        ServerAdmin webmaster@localhost
        DocumentRoot /var/www/react
        Header set Access-Control-Allow-Origin "*"

        # Available loglevels: trace8, ..., trace1, debug, info, notice, warn,
        # error, crit, alert, emerg.
        # It is also possible to configure the loglevel for particular
        # modules, e.g.
        #LogLevel info ssl:warn

        ErrorLog ${APACHE_LOG_DIR}/error.log
        CustomLog ${APACHE_LOG_DIR}/access.log combined

        # For most configuration files from conf-available/, which are
        # enabled or disabled at a global level, it is possible to
        # include a line for only one particular virtual host. For example the
        # following line enables the CGI configuration for this host only
        # after it has been globally disabled with "a2disconf".
        #Include conf-available/serve-cgi-bin.conf
</VirtualHost>

````