<?php

/**
 * @file
 * SFTP Storage class files for Storage API.
 */

/**
 * Provides an SFTP storage container.
 */
class StorageSFTP extends StorageContainer implements StorageContainerInterface {

  private $sftpSession;
  private $sftpStream;

  public function serviceSettingsDefault() {
    return array(
      'host' => '',
      'port' => '22',
      'username' => '',
      'auth_method' => 'password',
      'key_file_pub' => '',
      'key_file_private' => '',
      'key_file_passphrase' => '',
      'password' => '',
      'directory' => '',
      'base_url' => '',
    );
  }

  public function serviceSettingsForm($already_exists)
  {
    $items['host'] = array(
      '#type' => 'textfield',
      '#title' => t('Host'),
      '#description' => t("The SFTP server address. This parameter shouldn't have any trailing slashes and shouldn't be prefixed with sftp://"),
      '#required' => TRUE,
      '#default_value' => $this->settings['host'],
    );

    $items['port'] = array(
      '#type' => 'textfield',
      '#title' => t('Port'),
      '#description' => t('The standard SSH port is 22'),
      '#required' => TRUE,
      '#maxlength' => 5,
      '#size' => 5,
      '#default_value' => $this->settings['port'],
    );

    $items['username'] = array(
      '#type' => 'textfield',
      '#title' => t('Username'),
      '#description' => '',
      '#required' => TRUE,
      '#default_value' => $this->settings['username'],
    );

    $items['auth_method'] = array(
      '#type' => 'radios',
      '#title' => 'Auth method',
      '#options' => array(
        'password' => t('Password'),
        'keyfile' => t('Key file'),
      ),
      '#default_value' => $this->settings['auth_method']
    );

    $items['key_file_pub'] = array(
      '#title' => t('Public key'),
      '#type' => 'managed_file',
      '#default_value' => $this->settings['key_file_pub'],
      '#upload_location' => 'private://storage_api/sftp-keys/',
      '#states' => array(
        'visible' => array(
          'input[name="settings[auth_method]"]' => array('value' => 'keyfile'),
        ),
      ),
      '#upload_validators' => array(
        'file_validate_extensions' => array('public'),
      ),
    );

    $items['key_file_private'] = array(
      '#title' => t('Private key'),
      '#type' => 'managed_file',
      '#default_value' => $this->settings['key_file_private'],
      '#upload_location' => 'private://storage_api/sftp-keys/',
      '#states' => array(
        'visible' => array(
          'input[name="settings[auth_method]"]' => array('value' => 'keyfile'),
        ),
      ),
      '#upload_validators' => array(
        'file_validate_extensions' => array('private'),
      ),
    );

    $items['key_file_passphrase'] = array(
      '#type' => 'password',
      '#title' => t('Private key pass phrase'),
      '#deskription' => 'If private key file is encrypted (which it should be), the pass phrase must be provided.',
      '#default_value' => $this->settings['key_file_passphrase'],
      '#states' => array(
        'visible' => array(
          'input[name="settings[auth_method]"]' => array('value' => 'keyfile'),
        ),
      ),
    );

    $items['password'] = array(
      '#type' => 'password',
      '#title' => t('Password'),
      '#default_value' => $this->settings['password'],
      '#states' => array(
        'visible' => array(
          'input[name="settings[auth_method]"]' => array('value' => 'password'),
        ),
      ),
    );

    $items['directory'] = array(
      '#type' => 'textfield',
      '#title' => t('Directory'),
      '#description' => t('The folder to which the files will be uploaded on SFTP server'),
      '#required' => TRUE,
      '#default_value' => $this->settings['directory'],
    );

    if (!$this->access_control) {
      $items['base_url'] = array(
        '#type' => 'textfield',
        '#title' => t('Base URL'),
        '#description' => t('The HTTP path to access the directory specified above.'),
        '#required' => TRUE,
        '#default_value' => $this->settings['base_url'],
      );
    }

    return $items;
  }

  public function serviceSettingsValidate($already_exists)
  {
    if (!$this->settings['host']) {
      form_set_error('settings][host', t('Host field is required.'));
    }

    if (!$this->settings['port']) {
      form_set_error('settings][port', t('Port field is required.'));
    }

    if (!$this->settings['username']) {
      form_set_error('settings][username', t('Username field is required.'));
    }

    if ($this->settings['auth_method'] == 'password') {
      if (!$this->settings['password']) {
        form_set_error('settings][password', t('Password field is required.'));
      }
    }
    elseif ($this->settings['auth_method'] == 'keyfile') {
      form_set_error('settings', t('Key based auth not working now. If you want to help with fix - you\'re welcome :-)'));
      if (!$this->settings['key_file_pub']) {
        form_set_error('settings][key_file_pub', t('Public key file is required'));
      }
      if (!$this->settings['key_file_private']) {
        form_set_error('settings][key_file_private', t('Public key file is required'));
      }
    }

    if (!$this->settings['directory']) {
      form_set_error('settings][directory', t('Directory field is required.'));
    }

    if (!$this->access_control && !$this->settings['base_url']) {
      form_set_error('settings][base_url', t('Base URL field is required.'));
    }

    if (form_get_errors()) {
      return;
    }

    try {
      $this->login();
    }
    catch (StorageException $e) {
      form_set_error('settings', t($e->getMessage()));
      return;
    }

    try {
      if (!$already_exists) {
        $this->createDirectory($this->settings['directory']);
      }
      else {
          if (!$this->directoryExists($this->settings['directory'])) {
            form_set_error('settings][directory', t('Directory not exists!'));
          }
      }
    }
    catch (StorageException $e) {
      form_set_error('settings', $e->getMessage());
    }
  }

  public function serviceCreate()
  {
    $b = 1;
    try {
      $this->login();
      $this->createDirectory($this->settings['directory']);
    }
    catch (StorageException $e) {
      drupal_set_message($e->getMessage(), 'error');
    }
  }

  public function serviceInfo()
  {
    $info = array(
      t('Host') => $this->settings['host'],
      t('Port') => $this->settings['port'] ? $this->settings['port'] : 21,
      t('Username') => $this->settings['username'],
      t('Directory') => $this->settings['directory'],
    );

    if ($this->settings['auth_method'] == 'password') {
      $info[t('Auth method')] = t('password');
    }
    elseif ($this->settings['auth_method'] == 'keyfile') {
      $info[t('Auth method')] = t('key file');
    }

    $info[t('Base URL')] = url($this->settings['base_url'], array('absolute' => TRUE));

    return $info;
  }

  public function serviceDestroy()
  {
    $b = 1;
    try {
      $this->login();
      $this->removeDirectory($this->settings['directory']);
    }
    catch (StorageException $e) {
      drupal_set_message($e->getMessage(), 'error');
    }
  }

  /**
   * @inheritdoc
   */
  public function serviceInstanceCreate(Storage $storage, $uri)
  {
    try {
      $this->login();
      $remote_file = $this->settings['directory'] . DIRECTORY_SEPARATOR . $storage->filename;
      if (!@ssh2_scp_send($this->sftpSession, $uri, $remote_file)) {
        throw new StorageException('Can\'t write system file to storage!');
      }
      ssh2_sftp_chmod($this->sftpStream, $remote_file, 0644);
      return $storage->filename;
    }
    catch (StorageException $e) {
      drupal_set_message($e->getMessage(), 'error');
      return FALSE;
    }
  }

  public function serviceInstanceExists($reference)
  {
    $b = 1;
  }

  /**
   * @inheritdoc
   */
  public function serviceInstanceDestroy($reference)
  {
    $b = 1;
    $this->login();
    if (!ssh2_sftp_unlink($this->sftpStream, $this->settings['directory'].DIRECTORY_SEPARATOR.$reference)) {
      throw new StorageException('Can\'t remove '.$reference.' from storage.');
    }
  }

  /**
   * Login to the SSH server.
   *
   * @throws \StorageException
   */
  private function login() {
    if (isset($this->sftpSession)) {
      return;
    }

    $this->sftpSession = ssh2_connect($this->settings['host'], $this->settings['port']);

    if (!$this->sftpSession) {
      throw new StorageException('Can\'t connect to SSH server');
    }

    if ($this->settings['auth_method'] == 'password') {
      if (!ssh2_auth_password($this->sftpSession, $this->settings['username'], $this->settings['password'])) {
        throw new StorageException('Can\'t authenticate using this username and password');
      }
    }
    elseif ($this->settings['auth_method'] == 'keyfile') {
      $keys = file_load_multiple([$this->settings['key_file_pub'], $this->settings['key_file_private']]);
      $public_key_file = $keys[$this->settings['key_file_pub']];
      $private_key_file = $keys[$this->settings['key_file_private']];
      if (!ssh2_auth_pubkey_file($this->sftpSession, $this->settings['username'], $public_key_file->uri, $private_key_file->uri, $this->settings['key_file_passphrase'])) {
        throw new StorageException('Can\'t authenticate using this key');
      }
    }
    $this->sftpStream = ssh2_sftp($this->sftpSession);
  }

  /**
   * @inheritdoc
   */
  private function directoryExists($path) {
    $b = 1;
    if (!@ssh2_sftp_stat($this->sftpStream, $path)) {
      return FALSE;
    }
    else {
      return TRUE;
    }
  }

  /**
   * @inheritdoc
   */
  private function createDirectory($path) {
    $b = 1;
    if (!$this->directoryExists($path)) {
      if (@ssh2_sftp_mkdir($this->sftpStream, $path, 0777, true)) {
        return TRUE;
      }
      else {
        throw new StorageException('Can\'t create directory!');
      }
    }
    throw new StorageException('Directory already exists!');
  }

  /**
   * @inheritdoc
   */
  private function removeDirectory($path) {
    $command = 'rm -rf '.$path;
    if (@ssh2_exec($this->sftpSession, $command)) {
      return TRUE;
    }
    else {
      throw new StorageException('Can\'t remove directory "'.$this->settings['directory'].'"!');
    }
  }

  /**
   * {@inheritdoc}
   */
  public function serviceInstanceGetUri($reference) {
    $this->login();

    $temp_uri = new StorageTempURI();
    $result = ssh2_scp_recv($this->sftpSession, $this->settings['directory'].DIRECTORY_SEPARATOR.$reference, $temp_uri);

    if (!$result) {
      throw new StorageException();
    }

    return $temp_uri;
  }

  public function serviceInstanceCheck($reference)
  {
    $b = 1;
    return parent::serviceInstanceCheck($reference); // TODO: Change the autogenerated stub
  }

  public function serviceInstanceServe($reference, $https)
  {
    $url = $this->settings['base_url'].DIRECTORY_SEPARATOR.$reference;
    if (mb_strpos($this->settings['base_url'], 'http://') !== FALSE || mb_strpos($this->settings['base_url'], 'https://') !== FALSE) {
      if ($https) {
        $url = str_replace('http://', 'https://', $url);
      }
      else {
        $url = str_replace('https://', 'http://', $url);
      }
    }
    else {
      if ($https) {
        $url = 'http://'.$url;
      }
      else {
        $url = 'https://'.$url;
      }
    }
    return $url;
  }

  public function destroy()
  {
    $b = 1;
    parent::destroy(); // TODO: Change the autogenerated stub
  }

  public function instanceDestroy(Storage $storage, $reference)
  {
    $b = 1;
    parent::instanceDestroy($storage, $reference); // TODO: Change the autogenerated stub
  }

  public function serviceInstanceOutput($reference)
  {
    $b = 1;
    parent::serviceInstanceOutput($reference); // TODO: Change the autogenerated stub
  }

  public function serviceListReferences()
  {
    $b = 1;
    parent::serviceListReferences(); // TODO: Change the autogenerated stub
  }

}
