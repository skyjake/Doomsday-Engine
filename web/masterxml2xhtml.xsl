<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

  <xsl:output method="xml" omit-xml-declaration="yes" />

  <xsl:template match="serverchannel">
    <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="serverlist">
    <ul>
    <xsl:apply-templates />
    </ul>
  </xsl:template>

  <xsl:template match="channel">
      <div id="channel">
        <h1><xsl:value-of select="*[local-name()='generator']" /></h1>
        <h2 class="lastbuilddate">lastbuilddate: <xsl:value-of select="*[local-name()='pubdate']" /></h2><br />
        <p>Current status of all active Doomsday Engine game servers published to the central master server.</p>
        <img src="http://dengine.net/deng.png" />
        <p>managingeditor: webmaster@dengine.net</p>
        <p>webmaster: webmaster@dengine.net</p>
        <xsl:if test="*[local-name()='language']">
          <p>language: <xsl:value-of select="*[local-name()='language']" /></p>
        </xsl:if>
      </div>
  </xsl:template>

  <xsl:template match="masterserver">
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="gameinfo">Mode:<xsl:value-of select="*[local-name()='mode']" />
  IWAD: <xsl:value-of select="*[local-name()='iwad']" />
    <xsl:if test="*[local-name()='pwads']">
      PWADs: <xsl:value-of select="*[local-name()='pwads']" />
    </xsl:if>
  Map: <xsl:value-of select="*[local-name()='map']" />
  Setup: <xsl:value-of select="*[local-name()='setup']" />
  Players: <xsl:value-of select="*[local-name()='numplayers']" />/<xsl:value-of select="*[local-name()='maxplayers']" />
  </xsl:template>

  <xsl:template match="server">
    <li>
      <div class="serverinfo">
      <h2>
        <xsl:value-of select="*[local-name()='name']" /> - <xsl:value-of select="*[local-name()='info']" />
      </h2>
      <p>
        <xsl:value-of select="*[local-name()='ip']" />:<xsl:value-of select="*[local-name()='port']" />
      </p>
      <div class="gameinfo">
        <xsl:apply-templates select="gameinfo" />
      </div>
      </div>
    </li>
  </xsl:template>

</xsl:stylesheet>
