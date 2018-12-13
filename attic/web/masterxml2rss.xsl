<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

  <xsl:output method="xml" omit-xml-declaration="yes" />

  <xsl:template match="serverchannel">
    <xsl:apply-templates select="document(@src)" />
  </xsl:template>

  <xsl:template match="serverlist">
    <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="channel">
      <channel>
        <title>Doomsday Engine Master Server Feed</title>
        <link>http://dengine.net/masterfeed.xml</link>
        <description>RSS 2.0 feed listing the current status of all active Doomsday Engine game servers published to the central master server.</description>
        <image>
            <url>http://dengine.net/deng.png</url>
        </image>
        <managingeditor>webmaster@dengine.net</managingeditor>
        <webmaster>webmaster@dengine.net</webmaster>
        <generator><xsl:value-of select="*[local-name()='generator']" /></generator>
        <lastbuilddate><xsl:value-of select="*[local-name()='pubdate']" /></lastbuilddate>
        <xsl:if test="*[local-name()='language']">
          <language><xsl:value-of select="*[local-name()='language']" /></language>
        </xsl:if>
      </channel>
  </xsl:template>

  <xsl:template match="masterserver">
    <rss version="2.0"
      xmlns:atom="http://www.w3.org/2005/Atom"
      xmlns:dc="http://purl.org/dc/elements/1.1/"
      xmlns:content="http://purl.org/rss/1.0/modules/content/"
      xmlns:admin="http://webns.net/mvcb/"
      xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
      <xsl:apply-templates/>
    </rss>
  </xsl:template>

  <xsl:template match="gameinfo">Mode:<xsl:value-of select="*[local-name()='mode']" />
  IWAD:<xsl:value-of select="*[local-name()='iwad']" />
    <xsl:if test="*[local-name()='pwads']">
      PWADs:<xsl:value-of select="*[local-name()='pwads']" />
    </xsl:if>
  Map:<xsl:value-of select="*[local-name()='map']" />
  Setup:<xsl:value-of select="*[local-name()='setup']" />
  Players:<xsl:value-of select="*[local-name()='numplayers']" />/<xsl:value-of select="*[local-name()='maxplayers']" />
  </xsl:template>

  <xsl:template match="server">
    <item>
      <title>
        <xsl:value-of select="*[local-name()='name']" /> - <xsl:value-of select="*[local-name()='info']" />
      </title>
      <link>
        <xsl:value-of select="*[local-name()='ip']" />:<xsl:value-of select="*[local-name()='port']" />
      </link>
      <description>
        <xsl:apply-templates select="gameinfo" />
      </description>
    </item>
  </xsl:template>

</xsl:stylesheet>