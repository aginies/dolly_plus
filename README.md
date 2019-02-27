Dolly+

In administrating a large scale PC cluster, installation and updating both of kernel and utility software to whole the system are very troublesome, especially if the numbers of PC exceeds a hundred. In installation, people usually make dead copies of a hard disk image in which software systems are previously installed and then they are distributed among node PCs by CDs or hard disk themselves.  Though some software do such a process through networks, they commonly have an performance bottleneck at server where the original image are hold.

A cloning program `dolly' developed in ETH(Swiss Federal Institute of Technology) avoids such bottleneck by using a "ring" type connection rather than 'hub' type connection among one server and many clients .  I have extended its concept with multi-threading and pipeline technique.  It speeds up installation process very much. One-to-ten copying, for example, finishes in almost same minutes for one-to-one copy.

In addition, time out sensing 'bypass' mechanism makes the copy process pretty robust in the case of a client machine trouble.


Original Dolly page is here (dolly specific page is here )

infile /dev/sda102
outfile /dev/sda103
server cluster-master
firstclient node01
lastclient node09
clients 9
node01
node02
.....
node09
endconfig

<- input file name (in server)
<- output filename (in client)
<- the name of server
<- the name of the 1st client
<- the name of the last client
<- # of clients
<- names of the clients

dolly+ accept the same format of the dolly's configuration file. That is upper compatible. But also have some additional syntax.
this is an example of dolly+ specific configuration file.

 iofiles 3
 /dev/hda1 > /tmp/dev/hda1
 /data/file.gz >> /data/file
 boot.tar.Z >> /boot
 server cluster-master
 firstclient node01
 lastclient node09
 clients 9
 node01
 node02
            .....
 node09
 endconfig

 <- # of images for transfer
 <- *1
 <- *2
 <- *3
 <- the name of server
 <- dolly+ does not care, but must exist
 <- dolly+ does not care, but must exist
 <- names of the clients
 <- end code


*1: input file in the server is /dev/hda1 and output file in clients.
'>' means dolly+ does not modify the image.
*2: input file is /data/file.gz and output file is /data/file
'>>' indicate dolly+ should cook the file according to the name of the file.
Now. '.tar.', '.gz', 'tar.gz','tar.Z','cpio','cpio.gz','cpio.Z' are supported.
*3: input file is ./boot.tar.Z and output working directory is /boot.
the right argument of '>>' in cases where the input name is 'tar' and 'cpio' should be a directory name.

This version of dolly+ is just tested with multi thread library. But take one(ver.0.93) please.

From March, 2002, we started to use view CVS for `open' code management of dolly+. You can get the newest version of the software by the following procedure;

cvs login   (please do only once)
% cvs -d :pserver:anonymous@plateau.is.titech.ac.jp:/var/lib/cvs login
CVS password: [CR] ( just type CR only)

checkout
% cvs -d :pserver:anonymous@plateau.is.titech.ac.jp:/var/lib/cvs checkout dolly
We welcome for you to join the development. If you want join us, please send a mail to <takamiya@matsulab.is.titech.ac.jp>
with your account name and password ( encrypted by UNIX /etc/passwd format). In the case, `cvsroot' is
:pserver:<your_account_name>@plateau.is.titech.ac.jp:/var/lib/cvs

How to run dolly +
In server side    % dollyS -vf 'configuration-file'
In client side     % dollyC -v
(% is just a OS prompt)

I said that dolly+ is used for OS installation and upgrading, How to run dolly+ before an OS is installed.

Please mail me any kinds of bug reports. mail address is manabe@corvus.kek.jp

To discuss and exchange information about dolly+, we prepare a mailing list. Any comments , questions and suggestions are very welcomed.

Title: dolly+ ML <dolly@matsulab.is.titech.ac.jp>
A mailing list to discuss dolly+ development.
To join the list, please make a mail including lines as;
subscribe YOUR_NAME (NOT your E-mail address)

and send it to <dolly-ctl@matsulab.is.titech.ac.jp>. A confirmation mail will be replied. Please make your registration as an instruction in the replied mail. Registration is done automatically by a robotic mailing list server.

example:
subscribe Yasuhito TAKAMIYA

To unsubscribe from the mailing list, please send a mail to <dolly-ctl@matsulab.is.titech.ac.jp> including the bellow lines as its mail contents,

unsubscribe


Presentations

A presentation in 'Computer in High Energy Physics 2001 in Begin'

Change Log

12/9/2003 contribution by S.Takizawa
     When '#' appears in a lines of configuration file, then parser recognize the following as comments.
     Port numbers used in the previous version were fixed ones. Now you can select port numbers by using dolly+'s command argument options.

options:
--dataport portnum: 'portnum' number port is used for transration. dataport default is 9998.
--cntlport portnum 'portnum' number port is used for control. cntlport default is 9997.
--report portnum 'portnum' number port is used in Ping mode. report default is 9996.
-P portnum report is set as 'portnum', cntlport is set as 'portnum'+1, dataport is set as 'portnum'+2.
