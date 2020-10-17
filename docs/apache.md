# Apache Web Server


Install Apache2 and configure it:

````
a2enmod headers
sudo systemctl restart apache2.service

sudo ln -s ~/pi-sniffer/react/build /var/www/react

````

Edit the /etc/apache2/

````
sudo nano /etc/apache2/sites-available/000-default.conf

````

Change `DocumentRoot` and allow cors:

````
        DocumentRoot /home/ian/pi-sniffer/react/build
        Header set Access-Control-Allow-Origin "*"
````


Build `make cgijson`
Copy the cgi script over `sudo cp cgijson.cgi /usr/lib/cgi-bin


# Dependencies

npm

reactjs


react starter

font awesome
````
npm i --save @fortawesome/fontawesome-svg-core
  npm install --save @fortawesome/free-solid-svg-icons
  npm install --save @fortawesome/react-fontawesome
````