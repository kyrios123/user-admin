/*   mate-user-admin 
*   Copyright (C) 2018  zhuyaliang https://github.com/zhuyaliang/
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.

*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.

*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "user-info.h"
#include "user.h"
#include "user-base.h"
#include "user-share.h"
#include "user-face.h"
#include "user-list.h"

#define  LOCKFILE              "/tmp/user-admin.pid"
#define  APPICON               "user-admin.png"
#define  USER_ADMIN_PERMISSION "org.mate.user.admin.administration"

static gboolean on_window_quit (GtkWidget *widget, 
                                GdkEvent  *event, 
                                gpointer   user_data)
{
    UserAdmin *ua = (UserAdmin *)user_data;
    close_log_file();    
    g_slist_free_full (ua->UsersList,g_object_unref);
    gtk_main_quit();
    return TRUE;
}
static GdkPixbuf * GetAppIcon(void)
{
    GdkPixbuf *Pixbuf;
    GError    *Error = NULL;

    Pixbuf = gdk_pixbuf_new_from_file(ICONDIR APPICON,&Error);
    if(!Pixbuf)
    {
        MessageReport(_("Get Icon Fail"),Error->message,ERROR);
        mate_uesr_admin_log("Warning","mate-user-admin user-admin.png %s",Error->message);
        g_error_free(Error);
    }   
    
    return Pixbuf;
}    
static void UpdatePermission(UserAdmin *ua)
{
    gboolean  is_authorized;
    gboolean  self_selected;
    UserInfo *user;

    user = GetIndexUser(ua->UsersList,gnCurrentUserIndex);
    is_authorized = g_permission_get_allowed (G_PERMISSION (ua->Permission));
    self_selected = act_user_get_uid (user->ActUser) == geteuid ();
    
    gtk_widget_set_sensitive(ua->ButtonAdd,      is_authorized);
    gtk_widget_set_sensitive(ua->ButtonRemove,   is_authorized);
    gtk_widget_set_sensitive(ua->ButtonFace,     is_authorized);
    gtk_widget_set_sensitive(ua->EntryName,      is_authorized);
    gtk_widget_set_sensitive(ua->ComUserType,    is_authorized);
    gtk_widget_set_sensitive(ua->ButtonLanguage, is_authorized);
    gtk_widget_set_sensitive(ua->ButtonPass,     is_authorized);
    gtk_widget_set_sensitive(ua->SwitchAutoLogin,is_authorized);
    gtk_widget_set_sensitive(ua->ButtonUserTime, is_authorized);
    gtk_widget_set_sensitive(ua->ButtonUserGroup,is_authorized);
    if (is_authorized == 0 && self_selected == 1)
    {
        gtk_widget_set_sensitive(ua->ButtonFace,     self_selected);
        gtk_widget_set_sensitive(ua->EntryName,      self_selected);
        gtk_widget_set_sensitive(ua->ButtonUserTime, self_selected);
        gtk_widget_set_sensitive(ua->ButtonUserGroup,self_selected);
        
    }    

}    
static void on_permission_changed (GPermission *permission,
                                   GParamSpec  *pspec,
                                   gpointer     data)
{
    UserAdmin *ua = (UserAdmin *)data;
    UpdatePermission(ua);
}   
static void LoadHeader_bar(UserAdmin *ua)
{
    GtkWidget *header;
    
    header = gtk_header_bar_new ();
    gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (header), TRUE);
    gtk_header_bar_set_title (GTK_HEADER_BAR (header), _("Mate User Manage"));
    gtk_header_bar_pack_start (GTK_HEADER_BAR (header), ua->ButtonLock);
    gtk_window_set_titlebar (GTK_WINDOW (ua->MainWindow), header);
}    
static void InitMainWindow(UserAdmin *ua)
{
    GtkWidget *Window;
    GdkPixbuf *AppIcon;
    GError    *error = NULL;

    Window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    ua->MainWindow = Window;
    gtk_window_set_position(GTK_WINDOW(Window), GTK_WIN_POS_CENTER);
    gtk_window_set_title(GTK_WINDOW(Window),_("Mate User Manage")); 
    gtk_container_set_border_width(GTK_CONTAINER(Window),10);
    g_signal_connect(G_OBJECT(Window), 
                    "delete-event",
                     G_CALLBACK(on_window_quit),
                     ua);
    
    ua->Permission = polkit_permission_new_sync (USER_ADMIN_PERMISSION, NULL, NULL, &error);
    if (ua->Permission != NULL)
    {
        ua->ButtonLock = gtk_lock_button_new(ua->Permission);
        gtk_lock_button_set_permission(GTK_LOCK_BUTTON (ua->ButtonLock),ua->Permission);
        if(GetUseHeader() == 1)
        {    
            mate_uesr_admin_log("Info","mate-user-admin dialogs use header");
            LoadHeader_bar(ua);
        }
        gtk_widget_grab_focus(ua->ButtonLock);    
        g_signal_connect(ua->Permission, 
                        "notify",
                         G_CALLBACK (on_permission_changed), 
                         ua);
    
        AppIcon = GetAppIcon();
        if(AppIcon)
        {
            gtk_window_set_icon(GTK_WINDOW(Window),AppIcon);
            g_object_unref(AppIcon);
        }   
        ua->language_chooser = NULL;
    }
    else
    {    
        mate_uesr_admin_log ("Warning","Cannot create '%s' permission: %s", USER_ADMIN_PERMISSION, error->message);
        g_error_free (error);
    }
}

static void CreateInterface(GtkWidget *Vbox,UserAdmin *ua)
{
    GtkWidget *Hbox;
    GtkWidget *Hbox1;
    GtkWidget *Hbox2;

    Hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);  
    gtk_box_pack_start(GTK_BOX(Vbox),Hbox,FALSE,FALSE,0); 
    
    Hbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);  
    gtk_box_pack_start(GTK_BOX(Hbox),Hbox2,TRUE,TRUE,10); 
    gtk_widget_set_size_request (Hbox2, 200,-1);

    /*Display user list on the left side*/   
    DisplayUserList(Hbox2,ua);

    Hbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);  
    gtk_box_pack_start(GTK_BOX(Hbox),Hbox1,TRUE,TRUE,10); 
   
    /* Display the user's head image and name */
    DisplayUserSetFace(Hbox1,ua);
    
    /*user type and user password and user langusge and auto login and login time */    
    DisplayUserSetOther(Hbox1,ua);  

    /*Adding new users or remove users*/
    AddRemoveUser(Vbox,ua);

}
static int RecordPid(void)
{
    int pid = 0;
    int fd;
    int Length = 0; 
    char WriteBuf[30] = { 0 };
    fd = open(LOCKFILE,O_WRONLY|O_CREAT|O_TRUNC,0777);
    if(fd < 0)
    {
         MessageReport(_("open file"),_("Create pid file failed"),ERROR);
         return -1;      
    }       
    chmod(LOCKFILE,0777); 
    pid = getpid();
    sprintf(WriteBuf,"%d",pid);
    Length = write(fd,WriteBuf,strlen(WriteBuf));
    if(Length <= 0 )
    {
        MessageReport(_("write file"),_("write pid file failed"),ERROR);
        return -1;      
    }			
    close(fd);

    return 0;
}        
/******************************************************************************
* Function:              ProcessRuning      
*        
* Explain: Check whether the process has been started,If the process is not started, 
*          record the current process ID =====>"/tmp/user-admin.pid"
*        
* Input:         
*        
*        
* Output:  start        :TRUE
*          not start    :FALSE
*        
* Author:  zhuyaliang  31/07/2018
******************************************************************************/
static gboolean ProcessRuning(void)
{
    int fd;
    int pid = 0;
    gboolean Run = FALSE;
    char ReadBuf[30] = { 0 };

    if(access(LOCKFILE,F_OK) == 0)
    {
        fd = open(LOCKFILE,O_RDONLY);
        if(fd < 0)
        {
             MessageReport(_("open file"),_("open pid file failed"),ERROR);
             return TRUE;
        }        
        if(read(fd,ReadBuf,sizeof(ReadBuf)) <= 0)
        {
             MessageReport(_("read file"),_("read pid file failed"),ERROR);
             goto ERROREXIT;
        }        
        pid = atoi(ReadBuf);
        if(kill(pid,0) == 0)
        {        
             goto ERROREXIT;
        }
    }
    
    if(RecordPid() < 0)
        Run = TRUE;
    
    return Run;
ERROREXIT:
    close(fd);
    return TRUE;

}        

static void DeleteOldUserToList (ActUserManager *um, ActUser *ActUser, UserAdmin *ua)
{
    UserInfo *user;
    
    user = GetIndexUser(ua->UsersList,gnCurrentUserIndex);
    ua->UsersList = g_slist_remove(ua->UsersList,user);
    RefreshUserList(ua->UserList,ua->UsersList);
    /*Scavenging user information*/
    user = GetIndexUser(ua->UsersList,gnCurrentUserIndex);
    if(user == NULL)
    {
        g_error(_("No such user!!!"));
    }    
    UpdateInterface(user->ActUser,ua);                  
    ua->UserCount--;                                        
}

static void AddNewUserToList (ActUserManager *um, ActUser *ActUser, UserAdmin *ua)
{
    UserInfo   *user;
    UserInfo   *currentuser;
    const char *un;
    const char *rn;
	
    user = user_new();
    un = GetUserName(ActUser); 
    rn = GetRealName(ActUser);
    user->UserName = g_strdup(un);
    user->ActUser  = ActUser;
    UserListAppend(ua->UserList,
    			   DEFAULT,
                   rn,
                   un,
                   ua->UserCount,
                  &(ua->NUDialog->NewUserIter));
    user->Iter     = ua->NUDialog->NewUserIter;
    ua->UsersList  = g_slist_append(ua->UsersList,g_object_ref(user));
    currentuser    = GetIndexUser(ua->UsersList,gnCurrentUserIndex);
    UpdateInterface(currentuser->ActUser,ua);
    ua->UserCount +=1;//用户个数加1
}
static void users_loaded(ActUserManager  *manager,
                         GParamSpec      *pspec, 
                         UserAdmin       *ua)
{
    GtkWidget *fixed;
    GtkWidget *Vbox;
   
    ua->um = manager;
    ua->UserCount = GetUserInfo(ua);
    if(ua->UserCount < 0)
    {
        exit(0);
    }	
    
    fixed = gtk_fixed_new();
    gtk_container_add(GTK_CONTAINER(ua->MainWindow), fixed); 
  
    Vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);  
    gtk_fixed_put(GTK_FIXED(fixed),Vbox, 0, 0);
   
    CreateInterface(Vbox,ua);
    UpdatePermission(ua);
    g_signal_connect (manager, "user-added", G_CALLBACK (AddNewUserToList), ua);
    g_signal_connect (manager, "user-removed", G_CALLBACK (DeleteOldUserToList), ua);
    gtk_widget_show_all(ua->MainWindow);

}    
static void SetupUsersList(UserAdmin *ua)
{
    gboolean   loaded;
    ActUserManager *manager;
    manager = act_user_manager_get_default ();
   
    g_object_get (manager, "is-loaded", &loaded, NULL);
    if (loaded)
        users_loaded (manager,NULL,ua);
    else
        g_signal_connect(manager, 
                        "notify::is-loaded", 
                         G_CALLBACK (users_loaded), 
                         ua);

}    
int main(int argc, char **argv)
{
    UserAdmin ua;

    bindtextdomain (GETTEXT_PACKAGE,LUNAR_CALENDAR_LOCALEDIR);   
    textdomain (GETTEXT_PACKAGE); 
    
    gtk_init(&argc, &argv);
    
    /* Create the main window */
    InitMainWindow(&ua);

    /* Check whether the process has been started */
    if(ProcessRuning() == TRUE)
    {
        mate_uesr_admin_log("Info","The mate-user-admin process already exists");
        exit(0);        
    }
    WindowLogin = ua.MainWindow;
    /* Get local support language */ 

    SetupUsersList(&ua);
    gtk_main();

}
